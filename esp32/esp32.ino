#if !defined(ESP32)
#error This code is intended to run on the ESP32 platform! Please check your Tools->Board setting.
#endif

#define TIMER_INTERVAL_HZ 8000

#include "wolverine.h"
#include "main.hpp"

#include "driver/adc.h" //TODO refactor

// PwmOut pwm_out(AUDIOPIN_OUT);

/*************************************/

TaskHandle_t Task1;

int wolverine_pos = 0, available2play = 0;
// int call_in_progress=1;
// buffer for received audio
int audio_buffer_pos = 0, serial_buffer_pos=0, played_audio_pos = 1024 * 4;
int check_serial = 1;
int data2copy = 0;
int bytes2send = 0;

int test_positions[10];
int test_position_counter;

const int buf_sz = 1024 * 4;
char audio_buffer[buf_sz];  //orig buf_size = 1024*6
// buffer for audio to send
int audio2send_pos_w = 0, audio2send_sent = 0;
const int max_audio2send = 512, audio2send_buffer_sz = max_audio2send * 12;
// int available2write = audio2send_buffer_sz;
// char audio2send_buffer[audio2send_buffer_sz];
uint8_t audio2send_buffer[audio2send_buffer_sz];
char byte_buf;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE timerMuxS = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE bufferMuxS = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE new_audioMuxS = portMUX_INITIALIZER_UNLOCKED;

hw_timer_t* timer = NULL;

//FspTimer audio_timer, get_voice_timer; //gv

int did_play = -1;
int answer_len=0; //to read answer to commands sent to gsm modem

void ARDUINO_ISR_ATTR timer_callback() {

  if (call_in_progress){ //TODO non funziona, prende solo un sacco di rumore; controlla quello che arriva da internet
    portENTER_CRITICAL_ISR(&timerMux);

    // if ((audio_buffer_pos > played_audio_pos && (audio_buffer_pos - played_audio_pos > buf_sz/2)) ||
    if ((audio_buffer_pos != played_audio_pos )
        // (played_audio_pos > audio_buffer_pos && (buf_sz - played_audio_pos + audio_buffer_pos > buf_sz/2))
    ){

      portEXIT_CRITICAL_ISR(&timerMux);

      ledcWrite(AUDIOPIN_OUT, (uint8_t)(audio_buffer[played_audio_pos]));

      audio_buffer[played_audio_pos] = 0;

      played_audio_pos++;
      played_audio_pos = played_audio_pos % buf_sz;

    } else {
      portEXIT_CRITICAL_ISR(&timerMux);
    }
  }

  /********/

  // ledcWrite(AUDIOPIN_OUT, (audio_data[wolverine_pos++]));
  // wolverine_pos = wolverine_pos%sizeof(audio_data);

  //get audio to send
  // if (audio2send_pos_w <= audio2send_sent + max_audio2send*4){
  // if (available2write > 0){

  // portENTER_CRITICAL_ISR(&timerMuxS);
  //if we are in a call
  if (call_in_progress)  {
    portENTER_CRITICAL_ISR(&timerMuxS);
      audio2send_buffer[audio2send_pos_w++] = adc1_get_raw(ADC1_CHANNEL_0)/16; //(analogRead(AUDIOPIN_IN));
      audio2send_pos_w %= audio2send_buffer_sz;
    portEXIT_CRITICAL_ISR(&timerMuxS);

  }

  
}

/*************************************/

void taskloop(void * parameter);

void setup() {

  Serial.begin(9600);

  //pinMode(AUDIOPIN, OUTPUT);
  // ledcSetup(0, FREQUENCY, 8);
  ledcAttach(AUDIOPIN_OUT, FREQUENCY, 8);
  // pwm_out.begin(20000.0f, 0.0f);
  Serial.println("Setting pin...");
  pinMode(AUDIOPIN_IN, INPUT); //TODO rimuovi
  if (adc1_config_width(ADC_WIDTH_BIT_12) != ESP_OK){
    Serial.println("cannot configure adc width!!!");
    delay(5000);
  }
  adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_6); //gpio pin 36
  //can't use ADC 2 because it is used by wifi

  // Connect to Wi-Fi network
  // Set static IP configuration
  Serial.println("Configuring WiFi... TODO uncomment");
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

  // Print MAC address
  // Serial.print("MAC Address: ");
  // Serial.println(WiFi.macAddress());

  TSinit();

  Serial.println("Sim init...");
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // Create semaphore to inform us when the timer has fired
  // timerSemaphore = xSemaphoreCreateBinary();

  timer = timerBegin(1000000);  //timer resolution - 1Mhz
  timerAttachInterrupt(timer, &timer_callback);
  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter) with unlimited count = 0 (fourth parameter).
  timerAlarm(timer, 125, true, 0);  // repeat timer every 1/8000hz; unit is in useconds, thus 125 instead of 0.000125

  // stop the timer:
  /*
  if (timer) {
    // Stop and free timer
    timerEnd(timer);
    timer = NULL;
  }
  */
  Serial2.write("AT\n");
  Serial.println("Setup done");
  //beginTimerGetVoice(8000);

  /*
  auto config = pwm.defaultConfig();
  int pins[] = {AUDIOPIN_OUT};
  config.setPins(pins);
  //config.start_pin = PIN;
  config.copyFrom(info);
  pwm.begin(config);
  */

  // debug - send data to ts server in another thread
  xTaskCreatePinnedToCore(
      taskloop, /* Function to implement the task */
      "getdata", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &Task1,  /* Task handle. */
      0); /* Core where the task should run */
}

// char* testoutputcmd = "hello world!\n";

void taskloop(void * parameter){
  printf("started task loop\n");
  int read=0;
  for (;;) {
    //get, execute cmd
    read = 0;
    WiFiClient client = cmdServer.available();
    int canBreak=0;
    while (client.connected()) //TODO
      if (client.available()){
        printf("GOT CLIENT\n");
        printf("EXECUTING: ");
        read = client.read((uint8_t*)bufferloopcmd, BUFFERLOOPSIZE);

        // if (read){
          if (strncmp(bufferloopcmd, "!STOPTEXT!", 10) == 0){
            Serial2.write(26);
          }

          for (int i=0; i<read; i++){
            printf("%c,", bufferloopcmd[i]);
            Serial2.write(bufferloopcmd[i]);
          }
          // if (bufferloopcmd[read-1] != '\n'){
          printf("\n");
          Serial2.write("\n");
          // }
          //read answer and send output
          answer_len = 0;
          serial_buffer_pos = 0;
          unsigned long timeout = millis() + 100; // wait up to 'timeout' ms

          while (millis() < timeout || Serial2.available()) {
            while (Serial2.available()){
              serial_buffer[serial_buffer_pos++] = Serial2.read();
              // printf("  **getting char**\n");
              answer_len++;
              // refresh timeout
              timeout = millis() + 100;
            }
          }
          serial_buffer[serial_buffer_pos] = '\0';
          serial_buffer_pos=0;

          //send answer
          if (answer_len > 0){
            printf("OUTPUT OF COMMAND:\n%s\n", serial_buffer);
            printf("*** END ***\n", serial_buffer);
            client.write(serial_buffer, answer_len);
            // send_cmd_output(serial_buffer); //todo check return value
          } else {
            printf("NO OUTPUT FOR COMMAND\n");
          }

          break;
      // }
    }


    /*serial_buffer_pos = 0;
    // check data from internet
    int len = 0;
    portENTER_CRITICAL_ISR(&bufferMuxS);
    int type = get_audio_data(bufferloop, &len);
    portEXIT_CRITICAL_ISR(&bufferMuxS);
    switch (type) { //TODO aggiungi semaforo per new_audio e bufferloop
      case AUDIO:
        portENTER_CRITICAL_ISR(&new_audioMuxS);
        new_audio=1;
        portEXIT_CRITICAL_ISR(&new_audioMuxS);
        break;
      case COMMAND:
        len += 2;
        portENTER_CRITICAL_ISR(&bufferMuxS);
        memcpy(bufferloop, bufferloopcp, len); //BUFFERLOOPSIZE);
        portEXIT_CRITICAL_ISR(&bufferMuxS);
        // memcpy(serial_buffer, testoutputcmd, 13);
        //send command

        printf("cmd len: %i; writing ", len);
        for (int i=0; i<len; i++){
          printf("%c", bufferloopcp[i]);
          Serial2.write(bufferloopcp[i]);
        }
        printf("\n");
        Serial2.write("\n");
        //read answer
        while (Serial2.available()) {
          serial_buffer[serial_buffer_pos++] = Serial2.read();
          printf("%c.", serial_buffer[serial_buffer_pos-1]);
          answer_len++;
          // Serial.write(serial_buffer[serial_buffer_pos - 1]);
          if (serial_buffer[serial_buffer_pos - 1] == '\n' || serial_buffer[serial_buffer_pos - 1] == '\r') {
            serial_buffer[serial_buffer_pos] = '\0';
            printf("\n");
            // parse_serial(serial_buffer);
            serial_buffer_pos = 0;  //TODO
          }
        }

        //send answer
        printf("OUTPUT OF COMMAND\n %s\n", serial_buffer);
        send_cmd_output(serial_buffer); //todo check return value

        break;
      case -1:  //no packet
        // Serial.println("No packet received");
        break;
      default:
        Serial.print("unrecognized type ");
        Serial.println(type);
        break;
    }*/

    // check data from modem
    while (Serial2.available()) {
      serial_buffer[serial_buffer_pos++] = Serial2.read();
      Serial.write(serial_buffer[serial_buffer_pos - 1]);
      if (serial_buffer[serial_buffer_pos - 1] == '\n' || serial_buffer[serial_buffer_pos - 1] == '\r') {
        serial_buffer[serial_buffer_pos] = '\0';
        parse_serial(serial_buffer);
        serial_buffer_pos = 0;  //TODO
      }
    }

    while (Serial.available() && check_serial){
      char c = Serial.read();
      Serial.print(".");
      Serial.print(c);
      Serial2.write(c);
    }

  }
}

void play(int len, char* data) {
  /*for (int i=0; i < len; i++)
    printf("%i,", data[i]);
  printf("\n");*/
  //Serial.print("Playing bytes: "); Serial.println(len);

  for (int i = 0; i < len; i++) {
    //Serial.printf("%i,", packetBuffer[i]);
    auto t1 = micros();
    ledcWrite(AUDIOPIN_OUT, data[i]);

    //Serial.print((uint8_t)(data[i])); Serial.print(","); ///255.0f); Serial.print(",");

    // pwm_out.pulse_perc((((uint8_t)(data[i]))/255.0f) * 100);
    auto t2 = micros();
    delayMicroseconds(125 - (t2 - t1));  //125 = 1/freq * 1000000, where freq = 8000
  }
  //Serial.println("");
}

void play_wolverine() {  //FIXME
  printf("[DEBUG] playing 'wolverine!'\n");
  int time2sleep = 1000000 / 8000;
  for (int i = 0; i < sizeof(audio_data); i++) {
    //printf("%i,", audio_data[i]);
    //analogWrite(AUDIOPIN, audio_data[i]);
    auto t1 = micros();
    ledcWrite(AUDIOPIN_OUT, audio_data[i]); 
    // pwm_out.pulse_perc((audio_data[i]/255.0f) * 100);
    auto t2 = micros();
    delayMicroseconds(time2sleep - (t2 - t1));
    //delayMicroseconds(125 - (t2-t1));
  }
  ledcWrite(AUDIOPIN_OUT, 0); 
  // pwm_out.pulse_perc(0);
  printf("\n");
}

int justsent = 0;

void loop() {


  // if (did_play)
  //   printf("[DEBUG] did_play: %i, pos: %i\n", did_play, played_audio_pos);
  // send audio to ts
  if(call_in_progress){
    // printf("[DEBUG] call in progress\n");
    int ts1 = millis();
    int tmp = -1;
    // printf("[DEBUG] stop capture! audio2send_pos_w %i\n", audio2send_pos_w);
    // delay(50);
    portENTER_CRITICAL(&timerMuxS);
    tmp = audio2send_pos_w;
    // capt_audio = 0;
    portEXIT_CRITICAL(&timerMuxS);
    // printf("tmp: %i\n", tmp);
    // test_positions[test_position_counter++] = tmp;
    int audio2send = audio2send_sent < tmp ? (tmp - audio2send_sent)%max_audio2send : (audio2send_buffer_sz - audio2send_sent + tmp)%max_audio2send;
    // int audio2send = audio2send_sent < audio2send_pos_w ? (audio2send_pos_w - audio2send_sent)%max_audio2send : max_audio2send;

    // delay(5);
    // printf("[DEBUG] sent: %i pos_w: %i\n", audio2send_sent, tmp);
    // if (audio2send_sent + audio2send >= audio2send_buffer_sz){
    //   send_data(AUDIO, &(audio2send_buffer[audio2send_sent]), audio2send_buffer_sz - audio2send_sent);
    //   int audio_left = max_audio2send - (audio2send_buffer_sz - audio2send_sent);
    //   send_data(AUDIO, audio2send_buffer, audio_left);
    //   audio2send_sent = audio_left;
    // }
    // else {
    // printf("[DEBUG] sending %i bytes\n", audio2send);
    // printf("%i - %i, %i\n", tmp, audio2send_sent, audio2send);
    int audio2sendbk = audio2send;
    if (audio2send > 256){
      justsent = 1;
      if (audio2send_sent + audio2send >= audio2send_buffer_sz){
        // printf("%i + %i, max: %i\n", audio2send_sent, audio2send_buffer_sz - audio2send_sent, audio2send_buffer_sz);
        // printf("[DEBUG] sending %i\n", audio2send_buffer_sz - audio2send_sent);
        //portENTER_CRITICAL(&timerMuxS);
        send_data(AUDIO, &(audio2send_buffer[audio2send_sent]), audio2send_buffer_sz - audio2send_sent);
        //portEXIT_CRITICAL(&timerMuxS);

        audio2send = audio2send - (audio2send_buffer_sz - audio2send_sent);
        audio2send_sent = 0;
        // printf("%i + %i\n", audio2send_sent, audio2send);

      }
      // printf("[DEBUG] sending %i\n", audio2send);
      //portENTER_CRITICAL(&timerMuxS);
      send_data(AUDIO, &(audio2send_buffer[audio2send_sent]), audio2send);
      //portEXIT_CRITICAL(&timerMuxS);

      // printf("[DEBUG]: ");
      // for (int i=audio2send_sent; i < audio2send_sent + 10; i++){
      //   printf("%i, ", audio2send_buffer[audio2send_sent+i]);
      // }
      // printf("\n");

      audio2send_sent = (audio2send_sent + audio2send) % audio2send_buffer_sz;
    }
    
    // printf("[DEBUG] capture audio!\n");
    // portENTER_CRITICAL(&timerMuxS);
    // capt_audio = 1;
    // portEXIT_CRITICAL(&timerMuxS);
    
    int ts2 = millis();
    // if (justsent){
    //   printf("[DEBUG] sending %i took %i ms\n", audio2sendbk + audio2send_sent, ts2 - ts1);
    //   justsent = 0;
    // }

    // }

    // if (test_position_counter == 10){
    //   printf("[DEBUG] last 10 w pos:\n");
    //   for (int i=0; i < 10; i++){
    //     printf("%i, ", test_positions[i]);
    //   }
    //   test_position_counter = 0;
    //   printf("\n");
    // }

    // send_data(AUDIO, &(audio2send_buffer[audio2send_sent]), max_audio2send);
    // send_data(AUDIO, audio2send_buffer, max_audio2send);
    // audio2send_sent=(audio2send_sent+max_audio2send)%audio2send_buffer_sz; <===
    // if (audio2send_sent == 0) audio2send_pos_w = 0; <===
  }




  //play(600, &(audio_data[wolverine_pos]));
  //wolverine_pos = (wolverine_pos + 600)%sizeof(audio_data);
  /*
  int audio_left = wolverine_pos+512;
  fake_udp.setValue(&(audio_data[wolverine_pos]), audio_left > 512 ? 512 : audio_left);
  wolverine_pos = (wolverine_pos + audio_left) % sizeof(audio_data);
  copier.copy();
  */

  //get audio data
  if (call_in_progress){
    //play_wolverine();

    int new_audio_len = get_audio_data(bufferloop);
    // printf("[DEBUG] got %i byets of audio data\n", new_audio_len);
    // portENTER_CRITICAL_ISR(&new_audioMuxS);
    // if (new_audio_len>0)
    //   new_audio=1;
    // portEXIT_CRITICAL_ISR(&new_audioMuxS);
    // int tmp_new_audio=0, len=0;
    // portENTER_CRITICAL_ISR(&new_audioMuxS);
    // tmp_new_audio=new_audio;
    // len = new_audio_len;
    // portEXIT_CRITICAL_ISR(&new_audioMuxS);
    if (new_audio_len>0) {
        // Serial.println("I have received audio");
        //fake_udp.setValue((uint8_t*)bufferloop, len);
        //pwm.write((uint8_t*)bufferloop, len);
        // copier.copy();
        // play(len, bufferloop);
        /*
        //play(len, bufferloop);

        // if I have already played some of audio_buffer, or audio buffer is full, then play
        // if (played_audio_pos > 0 || audio_buffer_pos == buf_sz){
        if (audio_buffer_pos == buf_sz){
          Serial.println("Playing audio");
          //int b2play = buf_sz > played_audio_pos+60 ? 60 : buf_sz - played_audio_pos;
          //Serial.print("bytes to play, from played_audio_pos: "); Serial.print(b2play); Serial.print(", "); Serial.println(played_audio_pos);
          //Serial.print("and audio_buffer_pos is "); Serial.println(audio_buffer_pos);
          //play(b2play, &(audio_buffer[played_audio_pos]));

          for (int i = 0; i < buf_sz; i++){
            //Serial.printf("%i,", packetBuffer[i]);
            auto t1 = micros();
            //ledcWrite(AUDIOPIN_OUT, data[i]); TODO

            //Serial.print((uint8_t)(data[i])); Serial.print(","); ///255.0f); Serial.print(",");

            pwm_out.pulse_perc((((uint8_t)(audio_buffer[i]))/255.0f) * 100);
            auto t2 = micros();
            delayMicroseconds(125 - (t2-t1)); //125 = 1/freq * 1000000, where freq = 8000
          }

          //audio_buffer_pos = played_audio_pos;
          audio_buffer_pos = 0;
          //played_audio_pos = (played_audio_pos + len)%buf_sz;

        }
        Serial.println("Copying audio");
        */
        // TODO 1/3 if (audio_buffer_pos >= played_audio_pos || (audio_buffer_pos + len < played_audio_pos )){
        // if (played_audio_pos != 0){ //TODO not working good

        // printf("[DEBUG][NETWORK] %i/%i\n", audio_buffer_pos, buf_sz);

        // printf("played_audio_pos: %i, audio_buffer_pos: %i\n", played_audio_pos, audio_buffer_pos);
        
        portENTER_CRITICAL(&timerMux);
        data2copy = buf_sz - audio_buffer_pos > new_audio_len ? new_audio_len : buf_sz - audio_buffer_pos;
        // portENTER_CRITICAL(&timerMux);
        // if (played_audio_pos < audio_buffer_pos || (audio_buffer_pos + len) % buf_sz < played_audio_pos){


          // if (buf_left >= len){
          // printf("[DEBUG] writing %i\n", data2copy);
        memcpy(&(audio_buffer[audio_buffer_pos]), bufferloop, data2copy);
        // memcpy(&(audio_buffer[audio_buffer_pos]), bufferloop, len);
        // portENTER_CRITICAL(&timerMux);
        audio_buffer_pos += data2copy;
        audio_buffer_pos = audio_buffer_pos % buf_sz;
        portEXIT_CRITICAL(&timerMux);
        
        if (data2copy < new_audio_len){
          portENTER_CRITICAL(&timerMux);
          // printf("[DEBUG] writing %i\n", new_audio_len-data2copy);
          memcpy(&(audio_buffer[audio_buffer_pos]), &(bufferloop[data2copy]), new_audio_len-data2copy);
          
          audio_buffer_pos += new_audio_len - data2copy;
          portEXIT_CRITICAL(&timerMux);
        }

          // if (audio_buffer_pos != played_audio_pos )
          //   printf("[DEBUG] SHOULD PLAY AUDIOOOO\n");
          // else
          //   printf("[DEBUG] audio_buffer_pos = %i, played_audio_pos = %i\n", audio_buffer_pos, played_audio_pos);


        // new_audio=0;

        // } else
        //   portEXIT_CRITICAL(&timerMux);
        // if (audio_buffer_pos == 0) played_audio_pos = 0;
        // }

        // printf("data copied, audio_buffer_pos is %i and played_buffer_pos is %i \n", audio_buffer_pos, audio_buffer_pos);

        // TODO if - else scommenta (4/3)
        //   if (audio_buffer_pos >= played_audio_pos)
        //     available2play = audio_buffer_pos - played_audio_pos;
        //   else
        //     available2play = buf_sz - played_audio_pos + audio_buffer_pos;
        // } else {
        // printf("cannot copy data\n");

        // }
        //Serial.print("Copied bytes, audio_buffer_pos: "); Serial.print(len); Serial.print(", "); Serial.println(audio_buffer_pos);
    }
  }

  //free(bufferloop);

  // if (call_in_progress){ //TODO testa
  //   int data2read = 60;
  //   char v[data2read];
  //   for (int i = 0; i < data2read; i++){
  //     v[i] = analogRead(AUDIOPIN_IN)/4;
  //     delayMicroseconds(1000000/FREQUENCY);
  //   }
  //   send_data(AUDIO, v, data2read);
  // }

  /*
  // Check if data is available on UDP port 7000
  int packetSize = udp.parsePacket();
  if (packetSize) {
    //Serial.printf("Received %d bytes from %s, port %d\n", packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
    char packetBuffer[512];
    int len = udp.read(packetBuffer, 512);
    if (len > 0) {
      packetBuffer[len] = 0;

      //Serial.printf("UDP packet contents:\n");
      for (int i = 0; i < len; i++){
        //Serial.printf("%i,", packetBuffer[i]);
        auto t1 = micros();
        //analogWrite(AUDIOPIN, packetBuffer[i]);
        ledcWrite(0, packetBuffer[i]);
        auto t2 = micros();
        delayMicroseconds(125 - (t2-t1));
      }
      //Serial.printf("UDP packet end\n");
    }
    //analogWrite(AUDIOPIN, 0);
    //ledcWrite(0, 0);
  }

  //play_wolverine();
  //delay(1000);
  */
}
