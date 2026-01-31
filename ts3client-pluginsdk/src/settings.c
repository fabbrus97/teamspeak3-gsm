/*
*
* handles the loading of various settings
*
*/

#include "settings.h"
#include "utils/clog.h"
#include <string.h>

char* channel_to_connect;
int ts_audio_port; 
char* ucontroller_address;
int ucontroller_cmd_port = 0;
char* ts_ip_bind;
int ucontroller_audio_port;
int noise_cancelnoise;
char* noise_noiseprofilefile;
char* noise_noiserecordingfile;
int noise_suppr_level;
int allow_send_sms=1;
int allow_delete_sms=1;
int allow_make_call=1;
int allow_create_contacts=1;
int allow_delete_contacts=1;

void load_settings(){

	//first check the ini file
	const char* path = "/.config/teamspeak-gsm/settings.ini";
	const char *home = getenv("HOME"); // Get the value of $HOME
	char *fullpath = malloc(strlen(home) + strlen(path) + 1);
	strcpy(fullpath, home);
    strcat(fullpath, path);

	// ~/.local/share path
	const char* share_path = "/.local/share/teamspeak-gsm/";
	char* fullpath2 = malloc(strlen(home) + strlen(share_path) + 1);
	strcpy(fullpath2, home);
    strcat(fullpath2, share_path);

	// files in share path
	char* noiseprofile_fullpath = malloc(strlen(home) + strlen(share_path) + strlen("noiseprofile.bin") + 1); 
	strcpy(noiseprofile_fullpath, home);
	strcpy(noiseprofile_fullpath, share_path);
	strcat(noiseprofile_fullpath, "noiseprofile.bin");
	char* noiserecording_fullpath = malloc(strlen(home) + strlen(share_path) + strlen("noiserecording.pcm") + 1); 
	strcpy(noiseprofile_fullpath, home);
	strcpy(noiserecording_fullpath, share_path);
	strcat(noiserecording_fullpath, "noiserecording.pcm");

	dictionary  *   ini ;
	// const char* ini_channel;
	// const char* ini_ucontroller_ip;
	// const int   ini_ucontroller_at_port; //port where to send commands
    // const char* ini_ts_ip_bind; //ip to bind the ts plugin to; defaults to 0.0.0.0
    // const int   ini_ucontroller_audio_port; //this is the port where ucontroller will receive audio; defaults to 7000
	// const int   ini_ts_audio_port; //this is the port where ts will receive audio; defaults to 8000
	// const int   ini_noise_cancelnoise;
	// const char* ini_noise_noiseprofilefile;
	// const char* ini_noise_noiserecordingfile;
	// const char* ini_noise_noisesupprlevel;
	// const int ini_allow_send_sms;
	// const int ini_allow_delete_sms;
	// const int ini_allow_make_call;
	// const int ini_allow_create_contacts;
	// const int ini_allow_delete_contacts;

	ini = iniparser_load(fullpath);
    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", fullpath);
        // ini=fopen(fullpath, "w");

		// fprintf(ini,
		// "[ts]\n"
		// "\n"
		// "[ucontroller]\n"
		// "\n");
		// fclose(ini);
		ini = iniparser_load(fullpath2);
		if (ini == NULL){
	        fprintf(stderr, "cannot parse file: %s\n", fullpath2);
			exit(1);
		}
    } else {
		printf("[DEBUG] %s read correctly\n", fullpath);
	}
	iniparser_dump(ini, stderr); //dumps dictionary to stderr (prints the dict to a file pointer, in this case is stderr)
	const char* ini_channel = iniparser_getstring(ini, "ts:channel", "default");
	const char* ini_ucontroller_ip = iniparser_getstring(ini, "ucontroller:ip", "127.0.0.1");
	const int   ini_ucontroller_at_port = iniparser_getint(ini, "ucontroller:at_port", 8001);
    const char* ini_ts_ip_bind = iniparser_getstring(ini, "ts:ip", "0.0.0.0");
    const int   ini_ucontroller_audio_port = iniparser_getint(ini, "ucontroller:audio_port", 7000);
    const int   ini_ts_audio_port = iniparser_getint(ini, "ts:audio_port", 8000);
	const int   ini_noise_cancelnoise = iniparser_getint(ini, "noise:cancelnoise", 0);
	const char* ini_noise_noiseprofilefile = iniparser_getstring(ini, "noise:noiseprofilefile", noiseprofile_fullpath);
	const char* ini_noise_noiserecordingfile = iniparser_getstring(ini, "noise:noiserecordingfile", noiserecording_fullpath);
	const int ini_noise_noisesupprlevel = iniparser_getint(ini, "noise:noisesupprlevel", 6);
	const int ini_allow_send_sms = iniparser_getint(ini, "permissions:allow_send_sms", 0);
	const int ini_allow_delete_sms = iniparser_getint(ini, "permissions:allow_delete_sms", 0);
	const int ini_allow_make_call = iniparser_getint(ini, "permissions:allow_make_call", 0);
	const int ini_allow_create_contacts = iniparser_getint(ini, "permissions:allow_create_contacts", 0);
	const int ini_allow_delete_contacts = iniparser_getint(ini, "permissions:allow_delete_contacts", 0);

	//then check env variables
	const char* env_channel = getenv("TSGSM_TSCHANNEL"); // Get the value of $HOME
	const char* env_ucontroller_ip = getenv("TSGSM_UC_IP"); // Get the value of $HOME
	const char* env_ucontroller_at_port = getenv("TSGSM_UC_ATPORT"); //convert to int
    const char* env_ts_ip_bind = getenv("TSGSM_TS_BIND");
    const char* env_ucontroller_audio_port = getenv("TSGSM_UC_AUDIOPORT");//convert to int
    const char* env_ts_audio_port = getenv("TSGSM_TS_AUDIOPORT");//convert to int
	const char* env_noise_cancelnoise = getenv("TSGSM_NOISE_CANCELNOISE");//convert to int
	const char* env_noise_noiseprofilefile = getenv("TSGSM_NOISE_PROFILEPATH");
	const char* env_noise_noiserecordingfile = getenv("TSGSM_NOISE_RECORDINGPATH");
	const char* env_noise_noisesupprlevel = getenv("TSGSM_NOISE_SUPPRLEVEL");//convert to int
	const char* env_allow_send_sms = getenv("TSGSM_ALLOW_SEND_SMS");
	const char* env_allow_delete_sms = getenv("TSGSM_ALLOW_DELETE_SMS");
	const char* env_allow_make_call = getenv("TSGSM_ALLOW_MAKE_CALL");
	const char* env_allow_create_contacts = getenv("TSGSM_ALLOW_CREATE_CONTACTS");
	const char* env_allow_delete_contacts = getenv("TSGSM_ALLOW_DELETE_CONTACTS");


	ucontroller_cmd_port = env_ucontroller_at_port == NULL ? ini_ucontroller_at_port : atoi(env_ucontroller_at_port); 
    ucontroller_audio_port = env_ucontroller_audio_port == NULL ? ini_ucontroller_audio_port : atoi(env_ucontroller_audio_port); 
    ts_audio_port = env_ts_audio_port == NULL ? ini_ts_audio_port : atoi(env_ts_audio_port); 

    ts_ip_bind = malloc(17); 
	channel_to_connect = malloc(17);
	ucontroller_address = malloc(17);

    snprintf(ts_ip_bind, 17, "%s", env_ts_ip_bind == NULL ? ini_ts_ip_bind : env_ts_ip_bind);
    snprintf(channel_to_connect, 17, "%s", env_channel == NULL ? ini_channel : env_channel);
    snprintf(ucontroller_address, 17, "%s", env_ucontroller_ip == NULL ? ini_ucontroller_ip : env_ucontroller_ip);

	noise_cancelnoise = env_noise_cancelnoise == NULL ? ini_noise_cancelnoise : atoi(env_noise_cancelnoise);
	noise_noiseprofilefile = malloc(sizeof(char)*250);
	noise_noiserecordingfile = malloc(sizeof(char)*250);
	if (env_noise_noiseprofilefile != NULL){
		strcpy(noise_noiseprofilefile, env_noise_noiseprofilefile);
	} else {
		strcpy(noise_noiseprofilefile, ini_noise_noiseprofilefile);
	}
	if (env_noise_noiserecordingfile != NULL){
		strcpy(noise_noiserecordingfile, env_noise_noiserecordingfile);
	} else {
		strcpy(noise_noiserecordingfile, ini_noise_noiserecordingfile);
	}
	noise_suppr_level = env_noise_noisesupprlevel == NULL ? ini_noise_noisesupprlevel : atoi(env_noise_noisesupprlevel);

	allow_send_sms = env_allow_send_sms != NULL ? atoi(env_allow_send_sms) : ini_allow_send_sms;
	allow_delete_sms = env_allow_delete_sms != NULL ? atoi(env_allow_delete_sms) : ini_allow_delete_sms;
	allow_make_call = env_allow_make_call != NULL ? atoi(env_allow_make_call) : ini_allow_make_call;
	allow_create_contacts = env_allow_create_contacts != NULL ? atoi(env_allow_create_contacts) : ini_allow_create_contacts;
	allow_delete_contacts = env_allow_delete_contacts != NULL ? atoi(env_allow_delete_contacts) : ini_allow_delete_contacts;

	const char* env_loglevel = getenv("LOGLEVEL");
	if (env_loglevel != NULL) {
		if (strcasecmp(env_loglevel, "INFO") == 0) {
			clog_set_level(CLOG_INFO);
		} else if (strcasecmp(env_loglevel, "DEBUG") == 0) {
			clog_set_level(CLOG_DEBUG);
		} else if (strcasecmp(env_loglevel, "ERROR") == 0) {
			clog_set_level(CLOG_ERROR);
		} else {
			clog_set_level(CLOG_ERROR);
		}
	} else {
		clog_set_level(CLOG_ERROR);
	}

	iniparser_freedict(ini);
	free(fullpath);
	free(fullpath2);
	free(noiseprofile_fullpath);
	free(noiserecording_fullpath);
	

    printf("[DEBUG] ts ip to bind is ini/env/final: %s/%s/%s\n", ini_ts_ip_bind, env_ts_ip_bind, ts_ip_bind);
    printf("[DEBUG] ucontroller ip: %s cmd port: %i\n", ucontroller_address, ucontroller_cmd_port); //, ucontroller_cmd_port);
	printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

}
