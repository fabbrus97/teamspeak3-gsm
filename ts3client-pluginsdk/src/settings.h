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

extern char* channel_to_connect;
extern int ts_audio_port; //this is the port where ts will receive audio; defaults to 8000
extern char* ucontroller_address; //ip of the ucontroller; defaults to 127.0.0.1
extern int ucontroller_cmd_port; // port of the ucontroller where it is possible to send commands; defaults to 8001
extern char* ts_ip_bind; //ip to bind the ts plugin to; defaults to 0.0.0.0
extern int ucontroller_audio_port; //this is the port where ucontroller will receive audio; defaults to 7000
extern int noise_cancelnoise; //active noise canceling algorithm for audio incoming from the ucontroller? by default no
extern char* noise_noiseprofilefile; //where to save the noise computed profile
extern char* noise_noiserecordingfile; //raw recorded audio noise file (audio from the ucontroller)
extern int noise_suppr_level;

void load_settings();

#endif