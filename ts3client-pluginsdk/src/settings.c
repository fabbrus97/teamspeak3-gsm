/*
*
* handles the loading of various settings
*
*/

#include "settings.h"

char* channel_to_connect;
int ts_audio_port; 
char* ucontroller_address;
char* ucontroller_cmd_port;
char* ts_ip_bind;
int ucontroller_audio_port;
int noise_cancelnoise;
char* noise_noiseprofilefile;
char* noise_noiserecordingfile;
int noise_suppr_level;

void load_variables(){

	//first check the ini file
	const char* path = "/.config/teamspeak-gsm/settings.ini";
	const char *home = getenv("HOME"); // Get the value of $HOME
	char *fullpath = malloc(strlen(home) + strlen(path) + 1);
	strcpy(fullpath, home);
    strcat(fullpath, path);

	// ~/.local/share path
	const char* share_path = "/.local/share/teamspeak-gsm/";
	fullpath = malloc(strlen(home) + strlen(path) + 1);
	strcpy(fullpath, home);
    strcat(fullpath, share_path);

	// files in share path
	char* noiseprofile_fullpath = malloc(strlen(share_path) + strlen("noiseprofile.bin") + 1); 
	strcpy(noiseprofile_fullpath, share_path);
	strcat(noiseprofile_fullpath, "noiseprofile.bin");
	char* noiserecording_fullpath = malloc(strlen(share_path) + strlen("noiserecording.pcm") + 1); 
	strcpy(noiserecording_fullpath, share_path);
	strcat(noiserecording_fullpath, "noiserecording.pcm");

	dictionary  *   ini ;
	char* ini_channel;
	char* ini_ucontroller_ip;
	int   ini_ucontroller_at_port; //port where to send commands
    char* ini_ts_ip_bind; //ip to bind the ts plugin to; defaults to 0.0.0.0
    int   ini_ucontroller_audio_port; //this is the port where ucontroller will receive audio; defaults to 7000
	int   ini_ts_audio_port; //this is the port where ts will receive audio; defaults to 8000
	int   ini_noise_cancelnoise;
	char* ini_noise_noiseprofilefile;
	char* ini_noise_noiserecordingfile;
	char* ini_noise_noisesupprlevel;

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
    ini_ts_ip_bind = iniparser_getstring(ini, "ts:ip", "0.0.0.0");
    ini_ucontroller_audio_port = iniparser_getint(ini, "ucontroller:audio_port", 7000);
    ini_ts_audio_port = iniparser_getint(ini, "ts:audio_port", 8000);
	ini_noise_cancelnoise = iniparser_getint(ini, "noise:cancelnoise", 0);
	ini_noise_noiseprofilefile = iniparser_getstring(ini, "noise:noiseprofilefile", noiseprofile_fullpath);
	ini_noise_noiserecordingfile = iniparser_getstring(ini, "noise:noiserecordingfile", noiserecording_fullpath);
	ini_noise_noisesupprlevel = iniparser_getstring(ini, "noise:noisesupprlevel", 6);

	//then check env variables
	const char* env_channel = getenv("TSGSM_TSCHANNEL"); // Get the value of $HOME
	const char* env_ucontroller_ip = getenv("TSGSM_UC_IP"); // Get the value of $HOME
	const int env_ucontroller_at_port = getenv("TSGSM_UC_ATPORT"); 
    const char* env_ts_ip_bind = getenv("TSGSM_TS_BIND");
    const int env_ucontroller_audio_port = getenv("TSGSM_UC_AUDIOPORT");
    const int env_ts_audio_port = getenv("TSGSM_TS_AUDIOPORT");
	const int env_noise_cancelnoise = getenv("TSGSM_NOISE_CANCELNOISE");
	const char* env_noise_noiseprofilefile = getenv("TSGSM_NOISE_PROFILEPATH");
	const char* env_noise_noiserecordingfile = getenv("TSGSM_NOISE_RECORDINGPATH");
	const int* env_noise_noisesupprlevel = getenv("TSGSM_NOISE_SUPPRLEVEL");


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
	noise_suppr_level = env_noise_noisesupprlevel != NULL ? env_noise_noisesupprlevel : ini_noise_noisesupprlevel;

	iniparser_freedict(ini);

    printf("[DEBUG] ts ip to bind is ini/env/final: %s/%s/%s\n", ini_ts_ip_bind, env_ts_ip_bind, ts_ip_bind);

}
