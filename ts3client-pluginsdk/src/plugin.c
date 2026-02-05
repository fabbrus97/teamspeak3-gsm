/*
 * TS GSM
 * Simone Fabbri
 * <fabbrus@gmail.com> 
 * 19/12/2025
 * 
 * 
 * From ts demo
 * Copyright (c) TeamSpeak Systems GmbH
 */

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32)
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_rare_definitions.h"
#include "teamspeak/clientlib_publicdefinitions.h"
#include "plugin.h"
#include "utils/clog.h"
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 26

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128

#define PLUGIN_NAME "callbot"

clog_level_t clog_min_level = CLOG_ERROR;

const char* devID = "ts3callbotplayback";
const char* devDisplayName = "ts3_callbot_playback";
sem_t c_pb_sem;

int recording_noise = 0;
int recorded_noise_samples = 0;
sem_t noise_sem;

static uint8_t* denoised_audio_buffer = NULL;
static char* pluginID = NULL;

static struct TS3Functions ts3Functions;

pthread_t t1;
pthread_t t2;

/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

//this loop is responsible for receiving data via UDP, and playing it
void* main_loop_play(void* args);
//this loop is responsible for getting voice from ts server, and sending via UDP
void* main_loop_acquire(void* args);


/* Unique name identifying this plugin */
const char* ts3plugin_name() {
	return PLUGIN_NAME;
}

#define BUFFER_SIZE 10000
#define MIN_BUFFER 8000
short audio_buffer_to_ds[BUFFER_SIZE];
int data_in_buffer = 0;

#define audio_buffer_size 6144
size_t audio_buffer_pos = 0, audio_buffer_played=0;
short audio_buffer[audio_buffer_size];

/* Plugin version */
const char* ts3plugin_version() {
    return "1.0";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Simone Fabbri <fabbrus@gmail.com>";
}

/* Plugin description */
const char* ts3plugin_description() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "This plugin lets you make and receive phone calls";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

/*
 * Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
 * If the function returns 1 on failure, the plugin will be unloaded again.
 */
int ts3plugin_init() {
    char appPath[PATH_BUFSIZE];
    char resourcesPath[PATH_BUFSIZE];
    char configPath[PATH_BUFSIZE];
	char pluginPath[PATH_BUFSIZE];

    /* Your plugin init code here */
	// clog_set_level(CLOG_ERROR);

	const char* env_loglevel = getenv("LOGLEVEL");

	if (env_loglevel != NULL) {
		if (strcasecmp(env_loglevel, "INFO") == 0) {

			clog_set_level(CLOG_INFO);
		} else if (strcasecmp(env_loglevel, "DEBUG") == 0) {
			clog_set_level(CLOG_DEBUG);
		} else if (strcasecmp(env_loglevel, "ERROR") == 0) {
			clog_set_level(CLOG_ERROR);
		}
	}

    INFO("PLUGIN: init");

	//load variables
	load_settings();

	// 1. create audio playback device
	if (ts3Functions.registerCustomDevice(devID, devDisplayName, 8000, 1, 48000, 1) != ERROR_ok){
		ERROR("Error registering playback device");
		exit(1);
	}

    /* Example on how to query application, resources and configuration paths from client */
    /* Note: Console client returns empty string for app and resources path */
    ts3Functions.getAppPath(appPath, PATH_BUFSIZE);
    ts3Functions.getResourcesPath(resourcesPath, PATH_BUFSIZE);
    ts3Functions.getConfigPath(configPath, PATH_BUFSIZE);
	ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);

	INFO("PLUGIN: App path: %s\nResources path: %s\nConfig path: %s\nPlugin path: %s", appPath, resourcesPath, configPath, pluginPath);

	// printf("PID: %d — Waiting for debugger to attach... (send SIGCONT to continue)\n", getpid());
    // raise(SIGSTOP);  // This will pause the process until SIGCONT is received (e.g., from the debugger)

    return 0;  /* 0 = success, 1 = failure, -2 = failure but client will not show a "failed to load" warning */
	/* -2 is a very special case and should only be used if a plugin displays a dialog (e.g. overlay) asking the user to disable
	 * the plugin again, avoiding the show another dialog by the client telling the user the plugin failed to load.
	 * For normal case, if a plugin really failed to load because of an error, the correct return value is 1. */
}

//play data acquired from network (esp32) on ts client
void* main_loop_play(void* args){
	// start udp socket
	if (start_udp_socket() != 0){
		ERROR("Error starting udp socket");
		exit(1);
	}
	DEBUG(" ========== DEBUG THREAD STARTED ========== ");
	// uint8_t* mybuffer = NULL;
	const int bytes2play = 256;

	short chunk[bytes2play];
	struct timespec ts;
	long start = -1;
	for(;;){

		// free(mybuffer);

		/******** acquire custom playback ************
		 * see https://teamspeakdocs.github.io/PluginAPI/client_html/ar01s13s05.html

		 */
		size_t playbackBufferSize = 512*6; //I know this will be reduced to 512 due to downsample
		// I know it will take 512 * 1/8000 = 0.064 seconds to reproduce
		// so when I send more than 1 sec of playback in less than 1 sec of time, I pause sending voice
		short playbackBuffer[playbackBufferSize];

		if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
		// if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
			ERROR("clock_gettime: %s", strerror(errno));
			return NULL;
		}

		long now = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

		if (start < 0)
			start = now;

		// I always send 512 samples that, at 8khz, take 0.064 seconds to play
		// and I need to be a little bit faster than that
		if (now - start > 64){
			int error = ts3Functions.acquireCustomPlaybackData("ts3callbotplayback", playbackBuffer, playbackBufferSize);

			if(error == ERROR_ok) {
				send_voice(playbackBuffer, playbackBufferSize); 

			} else if(error == ERROR_sound_no_data) {
				// Not an error. The client lib has no playback data available. Depending on your custom sound API, either
				// pause playback for performance optimisation or send a buffer of zeros.
			} else {
				ERROR("Failed to get playback data");  // Error occured *
			}

			if (start != now)
				start = -1;

		} else {
			usleep(2000); //2 ms
		}
		

		sem_wait(&c_pb_sem);

        // Calculate how many samples are available in the circular buffer
        int available_samples = audio_buffer_pos - audio_buffer_played;
        if (available_samples < 0)
            available_samples += audio_buffer_size;  // wrap-around

        if (available_samples >= bytes2play) {
            // Copy a chunk of audio to local buffer
            for (int i = 0; i < bytes2play; ++i) {
                chunk[i] = audio_buffer[(audio_buffer_played + i) % audio_buffer_size];
				audio_buffer[(audio_buffer_played + i) % audio_buffer_size] = 0;
            }

            // Update play cursor
            audio_buffer_played = (audio_buffer_played + bytes2play) % audio_buffer_size;

            sem_post(&c_pb_sem);

            // Send to TeamSpeak
			ts3Functions.processCustomCaptureData("ts3callbotplayback", chunk, bytes2play);

		} else {
            sem_post(&c_pb_sem);
			usleep(1000);
		}
	}


}

//acquire data from from esp32
void* main_loop_acquire(void* args){

	uint8_t* mybuffer = NULL;
	size_t receivedBytes;
	size_t bytes2write;

	char* _noisefilepathtmp = malloc(sizeof(char)*100);
	char* noisefilepathtmp = malloc(sizeof(char)*100);
	char* noisefinalpath = malloc(sizeof(char)*100);
	// compute_file_path(noise_noiserecordingfile, _noisefilepathtmp);
	if (noise_noiserecordingfile[0] == '~') {
        const char *home = getenv("HOME"); // get home directory
        if (home) {
            snprintf(_noisefilepathtmp, (strlen(noise_noiserecordingfile) - 1) + strlen(home) + 1, "%s%s", home, noise_noiserecordingfile + 1);
            snprintf(noisefinalpath, (strlen(noise_noiserecordingfile) - 1) + strlen(home) + 1, "%s%s", home, noise_noiserecordingfile + 1);
        } else {
			strcpy(_noisefilepathtmp, noise_noiserecordingfile);
			strcpy(noisefinalpath, noise_noiserecordingfile);
		}
    }
	snprintf(noisefilepathtmp, strlen(_noisefilepathtmp) + strlen(".tmp") + 1, "%s.tmp", _noisefilepathtmp);
	
	int wasIrecordingBefore = 0;

	for(;;){
		sem_wait(&noise_sem);
		if (!recording_noise && wasIrecordingBefore){
			
			rename(noisefilepathtmp, noisefinalpath);
			wasIrecordingBefore = 0;
		}
		sem_post(&noise_sem);


		receivedBytes = receive_data(&mybuffer);

		sem_wait(&noise_sem);

		#if 0
		if(noise_cancelnoise && !recording_noise){

			// noised_audio_buffer = malloc(sizeof(uint8_t*)*receivedBytes);
			denoised_audio_buffer = malloc(sizeof(uint8_t*)*receivedBytes); //TODO non mi ricordo se la proporzione e' 1 a 1
			remove_noise(mybuffer, receivedBytes, denoised_audio_buffer);
			free(noised_audio_buffer);

		} else if (recording_noise){

			FILE* noisetmp = fopen(noisefilepathtmp, "a");
			
			assert(noisetmp != NULL);

			fwrite(mybuffer, sizeof(uint8_t), receivedBytes, noisetmp);
			recorded_noise_samples += receivedBytes;
			wasIrecordingBefore = 1;
			fclose(noisetmp);

		}
		#endif

		sem_post(&noise_sem);



		while (1){
			sem_wait(&c_pb_sem);
			if ((audio_buffer_pos + receivedBytes <= audio_buffer_played || audio_buffer_pos >= audio_buffer_played)){
				sem_post(&c_pb_sem);
				break;
			}
			sem_post(&c_pb_sem);

		}


		bytes2write = audio_buffer_size - audio_buffer_pos;
		bytes2write = bytes2write > receivedBytes ? receivedBytes : bytes2write;


		for (size_t i = 0; i < bytes2write; i++){
			
			sem_wait(&noise_sem);
			if (!noise_cancelnoise || recording_noise)
				audio_buffer[audio_buffer_pos+i] = (short) (mybuffer[i] - 0x80) << 8;
			else
				audio_buffer[audio_buffer_pos+i] = (short) (denoised_audio_buffer[i] - 0x80) << 8;
			sem_post(&noise_sem);
			

		}
		sem_wait(&c_pb_sem);
		audio_buffer_pos = (audio_buffer_pos + bytes2write) % audio_buffer_size;
		sem_post(&c_pb_sem);
			// free(&mybuffer); //TODO memory leak!
	}


}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
    /* Your plugin cleanup code here */
    INFO("PLUGIN: shutdown");

	//TODO clean in server.c
	/*
	 * Note:
	 * If your plugin implements a settings dialog, it must be closed and deleted here, else the
	 * TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
	 */

	/* Free pluginID if we registered it */
	if(pluginID) {
		free(pluginID);
		pluginID = NULL;
	}

	ts3Functions.unregisterCustomDevice(devID);
	pthread_cancel(t1);
	pthread_cancel(t2);

}

/****************************** Optional functions ********************************/
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */




/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
	return "!call";
}


/*
 * Implement the following three functions when the plugin should display a line in the server/channel/client info.
 * If any of ts3plugin_infoTitle, ts3plugin_infoData or ts3plugin_freeMemory is missing, the info text will not be displayed.
 */

/* Static title shown in the left column in the info frame */
const char* ts3plugin_infoTitle() {
	return "Ts call bot";
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data) {
	free(data);
}

/*
 * Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
 * the user manually disabled it in the plugin dialog.
 * This function is optional. If missing, no autoload is assumed.
 */
int ts3plugin_requestAutoload() {
	return 0;  /* 1 = request autoloaded, 0 = do not request autoload */
}


/************************** TeamSpeak callbacks ***************************/
/*
 * Following functions are optional, feel free to remove unused callbacks.
 * See the clientlib documentation for details on each function.
 */

/* Clientlib */

void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
    /* Some example code following to show how to use the information query functions. */

	DEBUG("IMPORTANT!!! THE IP OF UCONTR IS %s", ts_ip_bind);

    if(newStatus == STATUS_CONNECTION_ESTABLISHED) {  /* connection established and we have client and channels available */


		char* s;
        char msg[1024];
        anyID myID;
        uint64* ids;
        size_t i;
		unsigned int error;

        /* Print clientlib version */
        if(ts3Functions.getClientLibVersion(&s) == ERROR_ok) {
            INFO("PLUGIN: Client lib version: %s", s);
            ts3Functions.freeMemory(s);  /* Release string */
        } else {
            ERROR("Error querying client lib version");
            return;
        }

		/* Write plugin name and version to log */
        snprintf(msg, sizeof(msg), "Plugin %s, Version %s, Author: %s", ts3plugin_name(), ts3plugin_version(), ts3plugin_author());
        INFO(msg);

        /* Print virtual server name */
        if((error = ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &s)) != ERROR_ok) {
			if(error != ERROR_not_connected) {  /* Don't spam error in this case (failed to connect) */
				ERROR("Error querying server name");
			}
            return;
        }
        INFO("PLUGIN: Server name: %s", s);
        ts3Functions.freeMemory(s);

        /* Print virtual server welcome message */
        if(ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_WELCOMEMESSAGE, &s) != ERROR_ok) {
            ERROR("Error querying server welcome message");
            return;
        }
        INFO("PLUGIN: Server welcome message: %s", s);
        ts3Functions.freeMemory(s);  /* Release string */

        /* Print own client ID and nickname on this server */
        if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
            ERROR("Error querying client ID");
            return;
        }
        if(ts3Functions.getClientSelfVariableAsString(serverConnectionHandlerID, CLIENT_NICKNAME, &s) != ERROR_ok) {
            ERROR("Error querying client nickname");
            return;
        }
        INFO("PLUGIN: My client ID = %d, nickname = %s", myID, s);
        ts3Functions.freeMemory(s);

        /* Print list of all channels on this server */
        if(ts3Functions.getChannelList(serverConnectionHandlerID, &ids) != ERROR_ok) {
            ERROR("Error getting channel list");
            return;
        }
        INFO("PLUGIN: Available channels:");
        for(i=0; ids[i]; i++) {
            /* Query channel name */
            if(ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, ids[i], CHANNEL_NAME, &s) != ERROR_ok) {
                ERROR("Error querying channel name");
                return;
            }
            INFO("PLUGIN: Channel ID = %llu, name = %s", (long long unsigned int)ids[i], s);
            ts3Functions.freeMemory(s);
        }
        ts3Functions.freeMemory(ids);  /* Release array */

        /* Print list of existing server connection handlers */
        INFO("PLUGIN: Existing server connection handlers:");
        if(ts3Functions.getServerConnectionHandlerList(&ids) != ERROR_ok) {
            ERROR("Error getting server list");
            return;
        }
        for(i=0; ids[i]; i++) {
            if((error = ts3Functions.getServerVariableAsString(ids[i], VIRTUALSERVER_NAME, &s)) != ERROR_ok) {
				if(error != ERROR_not_connected) {  /* Don't spam error in this case (failed to connect) */
					ERROR("Error querying server name");
				}
                continue;
            }
            DEBUG("- %llu - %s", (long long unsigned int)ids[i], s);
            ts3Functions.freeMemory(s);
        }
        ts3Functions.freeMemory(ids);

		// // /* Open capture device we created earlier */
		if(ts3Functions.openCaptureDevice(serverConnectionHandlerID, "custom", devID) != ERROR_ok) {
			ERROR("Error opening capture device");
			exit(1);
		}

		// //open the device for playback
		DEBUG("======================= DEBUG =======================");
		DEBUG("======================= ===== =======================");
		char* defaultMode;

		if(ts3Functions.getDefaultPlayBackMode(&defaultMode) == ERROR_ok) {
			char*** array;

			if(ts3Functions.getPlaybackDeviceList(defaultMode, &array) == ERROR_ok) {
				for(i=0; array[i] != NULL; ++i) {
					DEBUG("Playback device name: %s", array[i][0]);  /* First element: Device name */
					DEBUG("Playback device ID: %s",   array[i][1]);  /* Second element: Device ID */

					/* Free element */
					ts3Functions.freeMemory(array[i][0]);
					ts3Functions.freeMemory(array[i][1]);
					ts3Functions.freeMemory(array[i]);
				}
				ts3Functions.freeMemory(array);  /* Free complete array */
			} else {
				ERROR("Error getting playback device list");
			}
		} else {
			ERROR("Error getting default playback mode");
		}
		char** array;

		if(ts3Functions.getPlaybackModeList(&array) == ERROR_ok) {
			for(i=0; array[i] != NULL; ++i) {
				DEBUG("Mode: %s", array[i]);
				ts3Functions.freeMemory(array[i]);  // Free C-string
			}
			ts3Functions.freeMemory(array);  // Free the array
		}

		if (ts3Functions.openPlaybackDevice(serverConnectionHandlerID, "custom", devID) != ERROR_ok){
			ERROR("Error opening playback device");
			exit(1);
		}

		//0 means the semaphore is shared between threads
		//1 is the initial value of the semaphore
		sem_init(&c_pb_sem, 0, 1);
		sem_init(&noise_sem, 0, 1);

		// pthread_create(&t1, NULL, main_loop_play, NULL);
		pthread_create(&t1, NULL, main_loop_play, ((void *)(&ts3Functions))); //TODO thread should be global, stop when disconneting
		pthread_create(&t2, NULL, main_loop_acquire, ((void *)(&ts3Functions))); //TODO thread should be global, stop when disconneting


		//INIT MY AT COMMANDS LIBRARY
		at_init(ucontroller_address, ucontroller_cmd_port);
    }
}

int ts3plugin_onTextMessageEvent(uint64 serverConnectionHandlerID, anyID targetMode, anyID toID, anyID fromID, const char* fromName, const char* fromUniqueIdentifier, const char* message, int ffIgnored) {
    DEBUG("ts3plugin_onTextMessageEvent!");
	DEBUG("PLUGIN: onTextMessageEvent %llu %d %d %s %s %d", (long long unsigned int)serverConnectionHandlerID, targetMode, fromID, fromName, message, ffIgnored);

	/* Friend/Foe manager has ignored the message, so ignore here as well. */
	if(ffIgnored) {
		return 0; /* Client will ignore the message anyways, so return value here doesn't matter */
	}

	//TODO check if command is enabled

	if (strncmp(message, ts3plugin_commandKeyword(), strlen(ts3plugin_commandKeyword())) == 0){
		DEBUG("got a command!!!!");
		anyID myID;
		if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
			ERROR("Error querying own client id");
			return 1;
		}

		//assume every command is an at command
		char* output = NULL;
		int at_message_len = strlen(message) - (strlen(ts3plugin_commandKeyword()) + 1);
		char at_message[at_message_len+1];
		strncpy(at_message, message + (strlen(ts3plugin_commandKeyword()) + 1), at_message_len);

		at_message[at_message_len] = '\0';
		int at_output = at_process_command(at_message, &output);
		DEBUG("output of at_command: %s", output);
		if (output == NULL) //we did not handle the command
			return -1; //ts3plugin_processCommand(serverConnectionHandlerID, message);
		else {
			DEBUG("I should reply: %s", output);

			uint64 myChannelID;
			/* Get own channel ID */
			if(ts3Functions.getChannelOfClient(serverConnectionHandlerID, myID, &myChannelID) != ERROR_ok) {
				ERROR("Error querying channel ID");
			}

			ts3Functions.requestSendChannelTextMsg(
				serverConnectionHandlerID,
				output,
				myChannelID,
				NULL
			);
			//TODO scommenta
			// free(output);
			DEBUG("at_output is %i", at_output);
			return at_output;
		}

	} else {
		DEBUG("message/keyword/kw/cmp: %s/%s/%li/%i", message, ts3plugin_commandKeyword(), strlen(ts3plugin_commandKeyword()), strncmp(message, ts3plugin_commandKeyword(), strlen(ts3plugin_commandKeyword())));
	}

    return 1;  /* 0 = handle normally, 1 = client will ignore the text message */
}