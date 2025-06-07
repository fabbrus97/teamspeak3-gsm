/*
*
* handles the loading of various settings
*
*/

#include "settings.h"

void load_variables(){

	//first check the ini file
	const char* path = "/.config/teamspeak-gsm/settings.ini";
	const char *home = getenv("HOME"); // Get the value of $HOME
	char *fullpath = malloc(strlen(home) + strlen(path) + 1);
	strcpy(fullpath, home);
    strcat(fullpath, path);

	dictionary  *   ini ;
	char* ini_channel;
	char* ini_ucontroller_ip;
	int ini_ucontroller_at_port; //port where to send commands
    char* ini_ts_ip_bind; //ip to bind the ts plugin to; defaults to 0.0.0.0
    int   ini_ucontroller_audio_port; //this is the port where ucontroller will receive audio; defaults to 7000

	ini = iniparser_load(fullpath);
    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", fullpath);
        ini=fopen(fullpath, "w");

		fprintf(ini,
		"[ts]\n"
		"\n"
		"[ucontroller]\n"
		"\n");
		fclose(ini);
		ini = iniparser_load(fullpath);
    } 
	iniparser_dump(ini, stderr); //dumps dictionary to stderr (prints the dict to a file pointer, in this case is stderr)
	ini_channel = iniparser_getstring(ini, "ts:channel", "default");
	ini_ucontroller_ip = iniparser_getstring(ini, "ucontroller:ip", "127.0.0.1");
	ini_ucontroller_at_port = iniparser_getint(ini, "ucontroller:at_port", 8001);
    ini_ts_ip_bind = iniparser_getstring(ini, "ucontroller:ip", "0.0.0.0");
    ini_ucontroller_audio_port = iniparser_getint(ini, "ucontroller:at_port", 7000);


	//then check env variables
	const char* env_channel = getenv("TSGSM_TSCHANNEL"); // Get the value of $HOME
	const char* env_ucontroller_ip = getenv("TSGSM_UC_IP"); // Get the value of $HOME
	const int env_ucontroller_at_port = getenv("TSGSM_UC_ATPORT"); 
    const char* env_ts_ip_bind = getenv("TSGSM_TS_BIND");
    const int env_ucontroller_audio_port = getenv("TSGSM_UC_AUDIOPORT");


	ucontroller_cmd_port = env_ucontroller_at_port == NULL ? ini_ucontroller_at_port : atoi(env_ucontroller_at_port); 
    ucontroller_audio_port = env_ucontroller_audio_port == NULL ? ini_ucontroller_audio_port : atoi(env_ucontroller_audio_port); 

    ts_ip_bind = malloc(17); 
	channel_to_connect = malloc(17);
	ucontroller_address = malloc(17);

    snprintf(ts_ip_bind, 17, "%s", env_ts_ip_bind == NULL ? ini_ts_ip_bind : env_ts_ip_bind);
    snprintf(channel_to_connect, 17, "%s", env_channel == NULL ? ini_channel : env_channel);
    snprintf(ucontroller_address, 17, "%s", env_ucontroller_ip == NULL ? ini_ucontroller_ip : env_ucontroller_ip);
	
	iniparser_freedict(ini);

    printf("[DEBUG] ts ip to bind is ini/env/final: %s/%s/%s\n", ini_ts_ip_bind, env_ts_ip_bind, ts_ip_bind);

}
