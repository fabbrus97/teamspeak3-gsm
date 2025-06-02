/*
*
* This file contains various settings 
* for the ts plugin and the microcontroller
* server. 
*
*/

#ifndef SETTINGS_H
#define SETTINGS_H

#include <iniparser/iniparser.h>

static char* channel_to_connect;
static int ts_audio_port; //this is the port where ts will receive audio; defaults to 8000
static char* ucontroller_address; //ip of the ucontroller; defaults to 127.0.0.1
static char* ucontroller_cmd_port; // port of the ucontroller where it is possible to send commands; defaults to 8001
static char* ts_ip_bind; //ip to bind the ts plugin to; defaults to 0.0.0.0
static int ucontroller_audio_port; //this is the port where ucontroller will receive audio; defaults to 7000

void load_settings();

#endif