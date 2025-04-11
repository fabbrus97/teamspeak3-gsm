/* This file handles the communication with the 
 *  TeamSpeak server. What it actually does is 
 *  sending and receiving UDP streams.
 */

#include "TSutils.hpp"

static char packetBuffer[UDP_READ];
static WiFiUDP udp;
// IPAddress serverAddress(192, 168, 1, 6); //define server address - tomahawk
IPAddress serverAddress(192, 168, 1, 19); //define server address - framework
// IPAddress serverAddress(192, 168, 180, 93);
uint16_t serverPort = 8000; //define server port

void TSinit(){
  // Start UDP server on port 7000
  udp.begin(7000);
  Serial.println("UDP server started on port 7000");
}

int get_data(char* data, int* datalen){
  int packetSize = udp.parsePacket();
  if (packetSize) {
    // printf("Received %d bytes from %s, port %d\n", packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
    
    int len = udp.read(packetBuffer, UDP_READ);
    if (len > 1) {
      //packetBuffer[len] = 0;
      //*data = (char*)malloc(sizeof(char)*len-1);
      memcpy(data, &(packetBuffer[1]), len-3);
      //data = &(packetBuffer[1]);
      // for (int i=1; i < len; i++){
      //   Serial.print((uint8_t)packetBuffer[i]); Serial.print(",");
      // }
      // Serial.println("");
      *datalen=len-3;
      //Serial.printf("packetBuffer[0] is %i\n", packetBuffer[0]);
      return packetBuffer[0];
    }
  }
  return -1;
}

int send_data(int type, const uint8_t* data, int size){
  // char packet[] = "ciao";
  // char packet[size+1]; 
  uint8_t packet[size];  //TODO should be char?
  // packet[0] = type; 
  // memcpy(&(packet[1]), data, size);
  memcpy(packet, data, size); //TODO fix type and uncomment
  
  // printf("Sending\n");
  // for (int i = 0; i < 10; i++){
  //   printf("%i, ", data[i]);
  // }
  // printf("\n");
  udp.beginPacket(serverAddress, serverPort);
  // size_t written = udp.write(packet, size+1);
  size_t written = udp.write(packet, size);
  // size_t written = udp.write(packet,  sizeof(packet));
  int res = udp.endPacket();
  // printf("packet sent with res: %i\n", res);
  // see https://github.com/espressif/arduino-esp32/issues/845
  // if (!res || written != sizeof(packet)) {
  //   printf("written: %i size: %i\n", written, sizeof(packet));
  //   Serial.println("Reinitialize UDP");
  //   udp.begin(serverPort);
  //   return -1;
  // }
  return 0;
}

int send_data_orig(int type, char* data, int size){
  // char packet[] = "ciao";
  // char packet[size+1]; 
  uint8_t packet[size];  //TODO should be char?
  // packet[0] = type; 
  // memcpy(&(packet[1]), data, size);
  // memcpy(packet, data, size); //TODO fix type and uncomment
  
  // printf("Sending\n");
  // for (int i = 0; i < 10; i++){
  //   printf("%i, ", data[i]);
  // }
  // printf("\n");
  udp.beginPacket(serverAddress, serverPort);
  // size_t written = udp.write(packet, size+1);
  size_t written = udp.write(packet, size);
  // size_t written = udp.write(packet,  sizeof(packet));
  int res = udp.endPacket();
  // see https://github.com/espressif/arduino-esp32/issues/845
  // if (!res || written != sizeof(packet)) {
  //   printf("written: %i size: %i\n", written, sizeof(packet));
  //   Serial.println("Reinitialize UDP");
  //   udp.begin(serverPort);
  //   return -1;
  // }
  return 0;
}
 
