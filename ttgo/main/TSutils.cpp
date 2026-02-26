/* This file handles the communication with the 
 *  TeamSpeak server. What it actually does is 
 *  sending and receiving UDP streams.
 */

#include "TSutils.hpp"

static char packetBuffer[UDP_READ];
IPAddress serverAddress(192, 168, 1, 14); //PUT YOUR SERVER ADDRESS HERE
uint16_t serverPort = 8078; //define server port - send the voice to be played on TS
uint16_t cmdPort = 8001;
static WiFiUDP udp;
WiFiServer cmdServer(cmdPort);
NetworkClient _client;

void TSinit(){
  // Start UDP server on port 7000
  udp.begin(7000); // get voice to be played on microphone
  Serial.println("UDP server started on port 7000");
  cmdServer.begin();
}

int get_audio_data(char* data){
  int packetSize = udp.parsePacket();
  if (packetSize) {
    
    
    int len = udp.read(packetBuffer, UDP_READ);
    if (len > 0) {
      memcpy(data, packetBuffer, len-2);
      return len-2;
    }
  }
  return -1;
}

int send_cmd_output(const char* data){
   if (!_client.connect(serverAddress, cmdPort)) {
    return -1;
  }

  _client.print(data);

  _client.stop();
}

int send_data(int type, const uint8_t* data, int size){
  uint8_t packet[size];
  memcpy(packet, data, size);
  
  udp.beginPacket(serverAddress, serverPort); 
  size_t written = udp.write(packet, size);
  int res = udp.endPacket();
  return 0;
}


 
