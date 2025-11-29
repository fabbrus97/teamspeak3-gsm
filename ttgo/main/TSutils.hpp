#ifndef TSUTILS_H
#define TSUTILS_H

#include <WiFi.h>
#include <WiFiUdp.h>

#define UDP_READ 1024

// possible commands 
#define AUDIO 0
#define COMMAND 1
// #define SMS 1
#define ADD_NUMBER
#define DEL_NUMBER
#define LIST_NUMBERS

extern WiFiServer cmdServer;

//init UDP server
void TSinit();
//receive udp data from server - voice, sms or commands
int get_audio_data(char* data);
int send_data(int type, const uint8_t* data, int size);
int send_cmd_output(const char* data);


#endif
