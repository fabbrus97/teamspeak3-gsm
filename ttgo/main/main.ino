/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cloud-mqtt-broker-sim800l/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

/*
 *
 * Defines and library imports
 *
*/

// #define DUMP_AT_COMMANDS
// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22
// for battery
#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00
#define AUDIOPIN_OUT         25 // gpio 32, pin 25

#include <SETTINGS.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <TinyGsmClient.h>
#include "esp_private/sar_periph_ctrl.h"
#include "esp_cpu.h"
#include "AudioIO.h"
#include "TSutils.hpp"
#include "SIMutils.hpp"
#include "main.h"


/*
 *
 * Global variables
 *
*/

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif
TinyGsmClient client(modem);
// I2C for SIM800 (to keep it running when powered from battery)
TwoWire I2CPower = TwoWire(0);

IPAddress staticIP(192, 168, 1, 18); // Set your static IP address here
IPAddress gateway(192, 168, 1, 1);     // Set your gateway IP address here
IPAddress subnet(255, 255, 255, 0);    // Set your subnet mask here

TaskHandle_t Task1;

//for command processing
#define BUFFERLOOPSIZE 1024
char bufferloopcmd[BUFFERLOOPSIZE];

/*
 * Audio ring buffers. Each one is single-producer/single-consumer and lock-free:
 * the timer ISR and loop() run on the same core, every index has exactly one
 * writer, and data is written before the index that publishes it.
 */
// TS -> phone: loop() writes audio_buffer_pos, the ISR writes played_audio_pos
const int buf_sz = AUDIO_SAMPLE_RATE * AUDIO_PLAY_BUFFER_MS / 1000;
uint8_t audio_buffer[buf_sz];
volatile int audio_buffer_pos = 0, played_audio_pos = 0;
// phone -> TS: the ISR writes audio2send_pos_w, loop() writes audio2send_sent
const int audio2send_buffer_sz = AUDIO_SAMPLE_RATE * AUDIO_SEND_BUFFER_MS / 1000;
const int audio2send_chunk = AUDIO_SAMPLE_RATE * AUDIO_SEND_CHUNK_MS / 1000;
uint8_t audio2send_buffer[audio2send_buffer_sz];
volatile int audio2send_pos_w = 0, audio2send_sent = 0;
char bufferloop[BUFFERLOOPSIZE];

#if AUDIO_STATS
volatile uint32_t st_isr_max_cycles = 0, st_adc_not_ready = 0, st_play_empty = 0, st_play_overrun = 0, st_cap_overrun = 0;
#endif

hw_timer_t* timer = NULL;

/*
 *
 * Functions
 *
*/

void taskloop(void * parameter);
// IRAM_ATTR explicitly: in core 3.x ARDUINO_ISR_ATTR is empty unless CONFIG_ARDUINO_ISR_IRAM is set,
// so the ISR used to run from flash cache and stall on cache misses
void timer_callback();

void IRAM_ATTR timer_callback() {
#if AUDIO_STATS
  uint32_t t0 = esp_cpu_get_cycle_count();
#endif

  // TS -> phone
  if (call_in_progress){
    int r = played_audio_pos;
    if (r != audio_buffer_pos){
      audio_dac_write(audio_buffer[r]);
      if (++r == buf_sz) r = 0;
      played_audio_pos = r;
    } else {
      STAT_ADD(st_play_empty, 1);
    }
  }

  // phone -> TS
  if (call_in_progress || recording_noise)  {
    uint8_t sample = audio_adc_read();
    int w = audio2send_pos_w;
    int next = w + 1;
    if (next == audio2send_buffer_sz) next = 0;
    if (next != audio2send_sent){
      audio2send_buffer[w] = sample;
      COMPILER_BARRIER();
      audio2send_pos_w = next;
    } else {
      STAT_ADD(st_cap_overrun, 1);
    }
  }

#if AUDIO_STATS
  uint32_t dt = esp_cpu_get_cycle_count() - t0;
  if (dt > st_isr_max_cycles) st_isr_max_cycles = dt;
#endif
}

bool setPowerBoostKeepOn(int en){
  I2CPower.beginTransmission(IP5306_ADDR);
  I2CPower.write(IP5306_REG_SYS_CTL0);
  if (en) {
    I2CPower.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    I2CPower.write(0x35); // 0x37 is default reg value
  }
  return I2CPower.endTransmission() == 0;
}

/*
 *
 * Main
 *
*/

void set_voice_paths(){
  int sleeptime = 500;
  SerialAT.write("AT+CLVL=100\r\n");
  delay(sleeptime);
  SerialAT.write("AT+CMIC=1,8\r\n");
  delay(sleeptime);
  SerialAT.write("AT+CAAS=0\r\n");
  delay(sleeptime);
  SerialAT.write("AT+CHF=0,1\r\n");
  delay(sleeptime);
}

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);
  delay(10);
  
  // Start I2C communication
  I2CPower.begin(I2C_SDA, I2C_SCL, 400000);
  
  // Keep power when running from battery
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);
  
  SerialMon.println("Wait...");

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();
  // modem.init();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modemInfo);

  Serial.println("Configuring WiFi...");
  WiFi.config(staticIP, gateway, subnet);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  TSinit();

  if (adc1_config_width(ADC_WIDTH_BIT_12) != ESP_OK){
    Serial.println("cannot configure adc width!!!");
    delay(5000);
  }
  // (this used to configure ADC1_CHANNEL_5 / GPIO33, but the ISR reads channel 6 / GPIO34)
  adc1_config_channel_atten(AUDIO_ADC_CHANNEL, AUDIO_ADC_ATTEN);
  // one read through the driver sets up controller, channel and width for the ISR
  adc1_get_raw(AUDIO_ADC_CHANNEL);
#if AUDIO_FAST_IO
  // the ISR starts conversions itself, so keep the SAR ADC powered permanently
  sar_periph_ctrl_adc_oneshot_power_acquire();
  adc_oneshot_ll_set_channel(ADC_UNIT_1, AUDIO_ADC_CHANNEL);
#endif

  // ledcAttach(AUDIOPIN_OUT, 24000, 8);
  dac_output_enable(DAC_CHAN_0);  // GPIO25

  // sample clock, started once ADC and DAC are ready
  timer = timerBegin(AUDIO_TIMER_HZ);
  timerAttachInterrupt(timer, &timer_callback);
  timerAlarm(timer, AUDIO_TIMER_HZ / AUDIO_SAMPLE_RATE, true, 0);  // one tick per sample, auto-reload
  Serial.printf("Audio: %d Hz, fast IO %d\n", AUDIO_SAMPLE_RATE, AUDIO_FAST_IO);

  set_voice_paths();

  xTaskCreatePinnedToCore(
      taskloop, /* Function to implement the task */
      "getdata", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &Task1,  /* Task handle. */
      0); /* Core where the task should run */

}


void taskloop(void * parameter){
  printf("started task loop\n");

  int answer_len=0; //to read answer to commands sent to gsm modem
  int read=0;
  for (;;) {
    //get, execute cmd
    read = 0;
    WiFiClient client = cmdServer.available();
    int canBreak=0;
    String input;
    while (client.connected()) //TODO
      if (client.available()){
        printf("GOT CLIENT\n");
        printf("EXECUTING: ");
        read = client.read((uint8_t*)bufferloopcmd, BUFFERLOOPSIZE);

        // if (read){
          if (strncmp(bufferloopcmd, "RNSTART", 7) == 0){
            printf("SETTING recording_noise TO 1\n");
            recording_noise = 1;
          }
          if (strncmp(bufferloopcmd, "RNSTOP", 6) == 0){
            printf("SETTING recording_noise TO 0\n");
            recording_noise = 0;
          }
          

          if (strncmp(bufferloopcmd, "!STOPTEXT!", 10) == 0){
            SerialAT.write(26);
          }

          if (strncmp(bufferloopcmd, "ATD", 3) == 0){
            call_in_progress = 1;
          }

          if (strncmp(bufferloopcmd, "PB_ONLY", 7) == 0){
            ACCEPT_ONLY_PB = bufferloopcmd[8] - '0';
          }


          for (int i=0; i<read; i++){
            printf("%c,", bufferloopcmd[i]);
            SerialAT.write(bufferloopcmd[i]);
          }
          // if (bufferloopcmd[read-1] != '\n'){
          printf("\n");
          SerialAT.write("\n");
          // }
          //read answer and send output
          unsigned long timeout = millis() + 100; // wait up to 'timeout' ms

          while (millis() < timeout || SerialAT.available()) {
            while (SerialAT.available()){
              input = SerialAT.readString();
              // refresh timeout
              timeout = millis() + 100;
            }
          }

          //send answer
          if (input.length() > 0){
            printf("OUTPUT OF COMMAND:\n%s\n", input);
            printf("*** END ***\n");
            client.write(input.c_str(), input.length());
            // send_cmd_output(serial_buffer); //todo check return value
          } else {
            printf("NO OUTPUT FOR COMMAND\n");
          }

          break;
      // }
    }

    // check data from modem
    while (SerialAT.available()) {
      String _input = SerialAT.readString();
      SerialMon.write(_input.c_str());
      parse_serial(_input.c_str()+2);
    }

    while (SerialMon.available()){
      char c = SerialMon.read();
      SerialMon.print(".");
      SerialMon.print(c);
      SerialAT.write(c);
    }

  }
}

size_t last_at_time = time(NULL);

void loop() {

  // phone -> TS: send everything buffered once at least AUDIO_SEND_CHUNK_MS is there
  if(call_in_progress || recording_noise){
    int sent = audio2send_sent;
    int backlog = audio2send_pos_w - sent;
    if (backlog < 0) backlog += audio2send_buffer_sz;

    if (backlog >= audio2send_chunk){
      while (backlog > 0){
        int n = backlog > AUDIO_MAX_PACKET ? AUDIO_MAX_PACKET : backlog;
        if (sent + n > audio2send_buffer_sz) n = audio2send_buffer_sz - sent;  // stop at the wrap
        send_data(AUDIO, &(audio2send_buffer[sent]), n);
        sent += n;
        if (sent == audio2send_buffer_sz) sent = 0;
        backlog -= n;
      }
      audio2send_sent = sent;  // hand the space back to the ISR
    }
  }

  // TS -> phone: queue received audio for the ISR
  if (call_in_progress){
    int new_audio_len = get_audio_data(bufferloop);
    if (new_audio_len > 0) {
      int w = audio_buffer_pos;
      int free_space = played_audio_pos - w - 1;
      if (free_space < 0) free_space += buf_sz;
      if (new_audio_len > free_space){  // full: drop the excess instead of overwriting unplayed audio
        STAT_ADD(st_play_overrun, new_audio_len - free_space);
        new_audio_len = free_space;
      }
      int first = buf_sz - w;
      if (first > new_audio_len) first = new_audio_len;
      memcpy(&(audio_buffer[w]), bufferloop, first);
      memcpy(&(audio_buffer[0]), &(bufferloop[first]), new_audio_len - first);
      w += new_audio_len;
      if (w >= buf_sz) w -= buf_sz;
      COMPILER_BARRIER();
      audio_buffer_pos = w;
    }
  }

#if AUDIO_STATS
  static uint32_t last_stats = 0;
  if ((call_in_progress || recording_noise) && millis() - last_stats > 5000){
    last_stats = millis();
    uint32_t isr_us10 = st_isr_max_cycles * 10 / getCpuFrequencyMhz();
    printf("[AUDIO] %d Hz | ISR max %lu.%lu us of %d us | adc not ready %lu | play empty ticks %lu | play overrun %lu | capture overrun %lu\n",
           AUDIO_SAMPLE_RATE, isr_us10 / 10, isr_us10 % 10, 1000000 / AUDIO_SAMPLE_RATE,
           st_adc_not_ready, st_play_empty, st_play_overrun, st_cap_overrun);
    st_isr_max_cycles = 0; st_adc_not_ready = 0; st_play_empty = 0; st_play_overrun = 0; st_cap_overrun = 0;
  }
#endif

    if (time(NULL) > last_at_time + 60){
      printf("sending AT to keep alive...\n");
      SerialAT.write("AT\r\n");
      last_at_time = time(NULL);
    }


}
