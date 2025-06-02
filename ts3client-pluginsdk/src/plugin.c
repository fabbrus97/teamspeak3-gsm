/*
 * TeamSpeak 3 demo plugin
 *
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
const char* devID = "ts3callbotplayback";
const char* devDisplayName = "ts3_callbot_playback";
sem_t c_pb_sem;

static char* pluginID = NULL;
static uint64 currentServerConnectionHandlerID;

static struct TS3Functions ts3Functions;

#ifdef _WIN32
/* Helper function to convert wchar_T to Utf-8 encoded strings on Windows */
static int wcharToUtf8(const wchar_t* str, char** result) {
	int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
	*result = (char*)malloc(outlen);
	if(WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
		*result = NULL;
		return -1;
	}
	return 0;
}
#endif

/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

//this loop is responsible for receiving data via UDP, and playing it
void* main_loop_play(void* arg);
//this loop is responsible for getting voice from ts server, and sending via UDP
void* main_loop_acquire(void* arg);


/* Unique name identifying this plugin */
const char* ts3plugin_name() {
#ifdef _WIN32
	/* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
	static char* result = NULL;  /* Static variable so it's allocated only once */
	if(!result) {
		const wchar_t* name = TEXT(PLUGIN_NAME)//L"Test Plugin";
		if(wcharToUtf8(name, &result) == -1) {  /* Convert name into UTF-8 encoded result */
			result = PLUGIN_NAME;  /* Conversion failed, fallback here */
		}
	}
	return result;
#else
	return PLUGIN_NAME;
#endif
}

#define BUFFER_SIZE 10000
#define MIN_BUFFER 8000
short audio_buffer_to_ds[BUFFER_SIZE];
int data_in_buffer = 0;

#define audio_buffer_size 6144
int audio_buffer_pos = 0, audio_buffer_played=0;
short audio_buffer[audio_buffer_size];

/* Plugin version */
const char* ts3plugin_version() {
    return "1.2";
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
    printf("PLUGIN: init\n");

	//load variables
	load_variables();

	// 1. create audio playback device
	if (ts3Functions.registerCustomDevice(devID, devDisplayName, 8000, 1, 48000, 1) != ERROR_ok){  //TODO check se la seconda frequenza dev'essere 8000 o 48000
		printf("Error registering playback device\n");
		exit(1);
	}



    /* Example on how to query application, resources and configuration paths from client */
    /* Note: Console client returns empty string for app and resources path */
    ts3Functions.getAppPath(appPath, PATH_BUFSIZE);
    ts3Functions.getResourcesPath(resourcesPath, PATH_BUFSIZE);
    ts3Functions.getConfigPath(configPath, PATH_BUFSIZE);
	ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);

	printf("PLUGIN: App path: %s\nResources path: %s\nConfig path: %s\nPlugin path: %s\n", appPath, resourcesPath, configPath, pluginPath);

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
		printf("Error starting udp socket");
		exit(1);
	}
	printf(" ========== DEBUG THREAD STARTED ========== ");
	// uint8_t* mybuffer = NULL;
	uint8_t* mybuffer = NULL;
	const int bytes2play = 256;

	struct timespec myts;
	myts.tv_sec=0;
	myts.tv_nsec = bytes2play*1000000000/8000;

	short chunk[bytes2play];
	struct timespec ts;
	long start = -1;
	for(;;){

		// free(mybuffer);

		/******** acquire custom playback ************
		 * see https://teamspeakdocs.github.io/PluginAPI/client_html/ar01s13s05.html

		 */
		// printf("**** acquire custom playback ***\n");
		size_t playbackBufferSize = 512*6; //I know this will be reduced to 512 due to downsample
		// I know it will take 512 * 1/8000 = 0.064 seconds to reproduce
		// so when I send more than 1 sec of playback in less than 1 sec of time, I pause sending voice
		short playbackBuffer[playbackBufferSize];
		// printf("[DEBUG] ACQUIRING VOICE\n");

		if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
		// if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
			perror("clock_gettime");
			return 1;
		}

		long now = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

		if (start < 0)
			start = now;

		// I always send 512 samples that, at 8khz, take 0.064 seconds to play
		// and I need to be a little bit faster than that
		// if (now - start == 0 || now - start > 64){
		if (now - start > 64){
			// printf("[DEBUG] now/start/diff %li/%li/%li\n", now, start, now - start);
			int error = ts3Functions.acquireCustomPlaybackData("ts3callbotplayback", playbackBuffer, playbackBufferSize);

			// int error = 0;
			// printf("error is %i\n", error);
			if(error == ERROR_ok) {

				// printf("Some audio playback available\n");

				// if (playbackBuffer != NULL){
					// printf("checking playbackbuffer: %p\n", playbackBuffer);
					// for (int i=0; i < 10; i++){
					// 	// printf("checking byte %i\n", i);
					// 	flag += playbackBuffer[i];
					// }

					// printf("\n");
					// Playback data available, send playbackBuffer to your custom device

						// printf("[DEBUG] i should send voice\n");
						send_voice(&playbackBuffer, playbackBufferSize, 1); //TODO check se non sia 2

					// else
						// printf("but that was a lie!\n");
				// }

			} else if(error == ERROR_sound_no_data) {
				// Not an error. The client lib has no playback data available. Depending on your custom sound API, either
				// pause playback for performance optimisation or send a buffer of zeros.
				// printf("No audio playback right now\n");
			} else {
				// printf("Failed to get playback data\n");  // Error occured *
			}

			if (start != now)
				start = -1;

		}
		/**/

		// usleep((playbackBufferSize*1000000)/48000);

		/********************** getting audio to be written and sent by other thread
		 * ****************/

		// sem_wait(&c_pb_sem);
		/*
		// printf("waiting for some audio to be written...\n");
		while (audio_buffer_pos >= audio_buffer_played && (audio_buffer_played + 2048) > audio_buffer_pos ){
		// while (audio_buffer_pos >= audio_buffer_played && (audio_buffer_played + bytes2play*8) > audio_buffer_pos ){
			// printf("cannot play %i to %i, audio_buffer_pos stuck to %i\n", audio_buffer_played, audio_buffer_played + bytes2play, audio_buffer_pos);
			sem_post(&c_pb_sem);
			usleep(200);
		}
		*/

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
            // ts3Functions.processCustomCaptureData(serverConnectionHandlerID, chunk, bytes2play);
			ts3Functions.processCustomCaptureData("ts3callbotplayback", chunk, bytes2play);

		} else {
            sem_post(&c_pb_sem);
			usleep(1000);
		}
		#if 0
		if ( (audio_buffer_played + bytes2play)%audio_buffer_size < audio_buffer_pos || audio_buffer_pos < audio_buffer_played){
		// if (1){

			// printf("[SEM_P] I need to play audio\n");

			// printf("\n");
			// printf("CONVERSION OK 💥💥💥\n");
			//TODO prova a commentare comando sopra per convertire, e manda audio a 48khz S16LE
			//NOTA S16LE richiede comunque una conversione (perche' da udp ricevi un byte alla volta,
			// devi leggerne due consecutivi e accorparli)
			// (*((const struct TS3Functions *)args)).processCustomCaptureData("ts3callbotplayback", audio_buffer, receivedBytes);
			printf("[P_INFO] Playing %i - %i\n", audio_buffer_played, audio_buffer_played+bytes2play);
			ts3Functions.processCustomCaptureData("ts3callbotplayback", &(audio_buffer[audio_buffer_played]), bytes2play);
			audio_buffer[audio_buffer_played] = 0;

			// nanosleep(&myts, &myts);
			// memset(&(audio_buffer[audio_buffer_played]), 0, bytes2play);
			sem_wait(&c_pb_sem);
			audio_buffer_played = (audio_buffer_played + bytes2play ) % audio_buffer_size;
			sem_post(&c_pb_sem);

			printf("[P_INFO] New audio position is %i\n", audio_buffer_played);
			// usleep((1*1000000)/48000);
		} else {
			sem_post(&c_pb_sem);
			printf("[DEBUG][SKIP PLAY] audio_buffer_played: %i, audio_buffer_pos: %i \n", audio_buffer_played, audio_buffer_pos);
		}
		#endif


		//
		// printf("[SEM_V] Audio played\n");

		// usleep((bytes2play*1000000)/48000); //~10666,66 periodico

		// usleep((bytes2play*1000000)/8000); //64000




	}


}

//acquire data from ts client, not from esp32!
void* main_loop_acquire(void* args){

	uint8_t* mybuffer = NULL;
	size_t receivedBytes;

	// size_t playbackBufferSize = 960; // 1024;
	size_t playbackBufferSize = 512*6;//360; // 1024; //TODO correlazione tra buffersize e usleep
	short playbackBuffer[playbackBufferSize];
	int i=0;
	// int error = ERROR_sound_no_data;
	struct timespec p;
	p.tv_sec=0;
	p.tv_nsec = 50000000L;
	int time2sleep = ((playbackBufferSize)*1000000/48000);
	int bytes2write = 0;
	for(;;){
		// printf("DEBUG another loop bites the dust %i\n", i);
		// usleep(20*1000); // sleep 30 ms
		//TODO // usleep(time2sleep); // sleep playbackBufferSize/frequency * 1000000 seconds (frequency is 48000, 1000000 is one second in nanoseconds used by usleep)
		// pthread_t         self;
		// self = pthread_self();
		// printf("my thread id is %i\n", self);

		receivedBytes = receive_data(&mybuffer);


		
		while (1){
			sem_wait(&c_pb_sem);
			if ((audio_buffer_pos + receivedBytes <= audio_buffer_played || audio_buffer_pos >= audio_buffer_played)){
				// printf("[DEBUG] I'm ready to write to buffer!\n");
				sem_post(&c_pb_sem);
				break;
			}
			sem_post(&c_pb_sem);
			// printf("[DEBUG] cannot write to buffer because not ready\n");
			printf("[DEBUG][SKIP WRITE] audio_buffer_played: %i, audio_buffer_pos: %i \n", audio_buffer_played, audio_buffer_pos);

			// usleep(200);
		}


		bytes2write = audio_buffer_size - audio_buffer_pos;
		bytes2write = bytes2write > receivedBytes ? receivedBytes : bytes2write;

		printf("[W_INFO] writing audio to %i - %i\n", audio_buffer_pos, audio_buffer_pos+bytes2write);

		// while ((audio_buffer_pos < audio_buffer_played && ((audio_buffer_pos + bytes2write) >= audio_buffer_played)) ){
		// 	usleep(100);
		// }

		// printf("[SEM_P] Writing %i bytes in buffer\n", bytes2write);

		for (int i = 0; i < bytes2write; i++){
			// printf("byte %i ok: %i\n", i, mybuffer);
			// printf("%i, ", mybuffer[i]);
			// uint8_t uintval = mybuffer[i];
			// short shortVal = uintval < 127 ? (1 - uintval/(127))*(-32768) : ((uintval - 127)/127)*(32768);
			audio_buffer[audio_buffer_pos+i] = (short) (mybuffer[i] - 0x80) << 8;
			// audio_buffer[i] = shortVal;
			// printf("%i => %i, ", mybuffer[i], audio_buffer[i]);


		}
		sem_wait(&c_pb_sem);
		audio_buffer_pos = (audio_buffer_pos + bytes2write) % audio_buffer_size;
		sem_post(&c_pb_sem);
			// printf("[SEM_V] Data written\n");

			
		// free(&mybuffer); //TODO memory leak!


		/* Get playback data from the client lib */
		// int error = (*((const struct TS3Functions *)args)).acquireCustomPlaybackData("ts3callbotplayback", playbackBuffer, playbackBufferSize);


		// printf("loop ended ok\n");

	}


}

void* main_loop_play_old(void* arg)
{
	/*
		https://www.geeksforgeeks.org/use-posix-semaphores-c/

		TODO ci sono delle variabili da osservare:
			- buffer dei messaggi
			- buffer delle chiamate
		può convenire fare due thread
	*/
	// Clean buffers:
    // memset(server_message, '\0', sizeof(server_message));

	int socket_desc;
	struct sockaddr_in server_addr, client_addr;
	char /* server_message[2000], */ client_message[6000];
	int client_struct_length; // = sizeof(client_addr);

    client_struct_length = sizeof(client_addr);
    memset(client_message, '\0', sizeof(client_message));

    // Create UDP socket:
    socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(socket_desc < 0){
        printf("Error while creating socket\n");
        return -1;
    }
    printf("Socket created successfully\n");

    // Set port and IP:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ts_audio_port);
    server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");

    // Bind to the set port and IP:
    if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        printf("Couldn't bind to the port\n");
        exit(1);
    }
    printf("Done with binding\n");

    printf("Listening for incoming messages...\n\n");

	int lenRecv;
	for(;;){
		lenRecv = recvfrom(socket_desc, client_message, sizeof(client_message), NULL,
            (struct sockaddr*)&client_addr, &client_struct_length);
		printf("i have received: %i\n", lenRecv);
        if (lenRecv < 0){
            printf("Couldn't receive\n");
			continue;
        }
        printf("Received message from IP: %s and port: %i\n",
              inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // int buffer_length = (int)(lenRecv/2 + 0.5)+100;
		short buffer[lenRecv];
		for (int i = 0; i < lenRecv; i++){
			uint8_t uintVal = client_message[i];
			short shortVal = uintVal < 127 ? (1 - uintVal/(127))*(-32768) : ((uintVal - 127)/127)*(32768);
			buffer[i] = shortVal;
		}
        ts3Functions.processCustomCaptureData(devID, buffer, lenRecv);

        struct timespec myts;
        myts.tv_sec=0;
        myts.tv_nsec = 10000; //10 millisec
        nanosleep(&myts, &myts);

    }
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
    /* Your plugin cleanup code here */
    printf("PLUGIN: shutdown\n");

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
}

/****************************** Optional functions ********************************/
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure() {
	printf("PLUGIN: offersConfigure\n");
	/*
	 * Return values:
	 * PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
	 * PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
	 * PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
	 */
	return PLUGIN_OFFERS_NO_CONFIGURE;  /* In this case ts3plugin_configure does not need to be implemented */
}

/* Plugin might offer a configuration window. If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
void ts3plugin_configure(void* handle, void* qParentWidget) {
    printf("PLUGIN: configure\n");
}

/*
 * If the plugin wants to use error return codes, plugin commands, hotkeys or menu items, it needs to register a command ID. This function will be
 * automatically called after the plugin was initialized. This function is optional. If you don't use these features, this function can be omitted.
 * Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
 */
void ts3plugin_registerPluginID(const char* id) {
	const size_t sz = strlen(id) + 1;
	pluginID = (char*)malloc(sz * sizeof(char));
	_strcpy(pluginID, sz, id);  /* The id buffer will invalidate after exiting this function */
	printf("PLUGIN: registerPluginID: %s\n", pluginID);
}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword() {
	return "!test";
}

static void print_and_free_bookmarks_list(struct PluginBookmarkList* list)
{
    int i;
    for (i = 0; i < list->itemcount; ++i) {
        if (list->items[i].isFolder) {
            printf("Folder: name=%s\n", list->items[i].name);
            print_and_free_bookmarks_list(list->items[i].folder);
            ts3Functions.freeMemory(list->items[i].name);
        } else {
            printf("Bookmark: name=%s uuid=%s\n", list->items[i].name, list->items[i].uuid);
            ts3Functions.freeMemory(list->items[i].name);
            ts3Functions.freeMemory(list->items[i].uuid);
        }
    }
    ts3Functions.freeMemory(list);
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command) { //TODO
	char buf[COMMAND_BUFSIZE];
	char *s, *param1 = NULL, *param2 = NULL;
	int i = 0;
	enum { CMD_NONE = 0, CMD_JOIN, CMD_COMMAND, CMD_SERVERINFO, CMD_CHANNELINFO, CMD_AVATAR, CMD_ENABLEMENU, CMD_SUBSCRIBE, CMD_UNSUBSCRIBE, CMD_SUBSCRIBEALL, CMD_UNSUBSCRIBEALL, CMD_BOOKMARKSLIST } cmd = CMD_NONE;
#ifdef _WIN32
	char* context = NULL;
#endif

	printf("PLUGIN: process command: '%s'\n", command);

	_strcpy(buf, COMMAND_BUFSIZE, command + strlen(ts3plugin_commandKeyword())+1); // skip the commandkeyword and the space
#ifdef _WIN32
	s = strtok_s(buf, " ", &context);
#else
	s = strtok(buf, " ");
#endif
	while(s != NULL) {
		printf("[DEBUG] checking s %s\n", s);
		if(i == 0) {
			if(!strcmp(s, "join")) {
				cmd = CMD_JOIN;
			} else if(!strcmp(s, "command")) {
				cmd = CMD_COMMAND;
			} else if(!strcmp(s, "serverinfo")) {
				cmd = CMD_SERVERINFO;
			} else if(!strcmp(s, "channelinfo")) {
				printf("[DEBUG] strcmp positive for 'channelinfo'\n");
				cmd = CMD_CHANNELINFO;
			} else if(!strcmp(s, "avatar")) {
				cmd = CMD_AVATAR;
			} else if(!strcmp(s, "enablemenu")) {
				cmd = CMD_ENABLEMENU;
			} else if(!strcmp(s, "subscribe")) {
				cmd = CMD_SUBSCRIBE;
			} else if(!strcmp(s, "unsubscribe")) {
				cmd = CMD_UNSUBSCRIBE;
			} else if(!strcmp(s, "subscribeall")) {
				cmd = CMD_SUBSCRIBEALL;
			} else if(!strcmp(s, "unsubscribeall")) {
				cmd = CMD_UNSUBSCRIBEALL;
            } else if (!strcmp(s, "bookmarkslist")) {
                cmd = CMD_BOOKMARKSLIST;
            }
		} else if(i == 1) {
			param1 = s;
		} else {
			param2 = s;
		}
#ifdef _WIN32
		s = strtok_s(NULL, " ", &context);
#else
		s = strtok(NULL, " ");
#endif
		i++;
	}

	switch(cmd) {
		case CMD_NONE:
			return 1;  /* Command not handled by plugin */
		case CMD_JOIN:  /* /test join <channelID> [optionalCannelPassword] */
			if(param1) {
				uint64 channelID = (uint64)atoi(param1);
				char* password = param2 ? param2 : "";
				char returnCode[RETURNCODE_BUFSIZE];
				anyID myID;

				/* Get own clientID */
				if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
					ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
					break;
				}

				/* Create return code for requestClientMove function call. If creation fails, returnCode will be NULL, which can be
				 * passed into the client functions meaning no return code is used.
				 * Note: To use return codes, the plugin needs to register a plugin ID using ts3plugin_registerPluginID */
				ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);

				/* In a real world plugin, the returnCode should be remembered (e.g. in a dynamic STL vector, if it's a C++ plugin).
				 * onServerErrorEvent can then check the received returnCode, compare with the remembered ones and thus identify
				 * which function call has triggered the event and react accordingly. */

				/* Request joining specified channel using above created return code */
				if(ts3Functions.requestClientMove(serverConnectionHandlerID, myID, channelID, password, returnCode) != ERROR_ok) {
					ts3Functions.logMessage("Error requesting client move", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
				}
			} else {
				ts3Functions.printMessageToCurrentTab("Missing channel ID parameter.");
			}
			break;
		case CMD_COMMAND:  /* /test command <command> */
			if(param1) {
				/* Send plugin command to all clients in current channel. In this case targetIds is unused and can be NULL. */
				if(pluginID) {
					/* See ts3plugin_registerPluginID for how to obtain a pluginID */
					printf("PLUGIN: Sending plugin command to current channel: %s\n", param1);
					ts3Functions.sendPluginCommand(serverConnectionHandlerID, pluginID, param1, PluginCommandTarget_CURRENT_CHANNEL, NULL, NULL);
				} else {
					printf("PLUGIN: Failed to send plugin command, was not registered.\n");
				}
			} else {
				ts3Functions.printMessageToCurrentTab("Missing command parameter.");
			}
			break;
		case CMD_SERVERINFO: {  /* /test serverinfo */
			/* Query host, port and server password of current server tab.
			 * The password parameter can be NULL if the plugin does not want to receive the server password.
			 * Note: Server password is only available if the user has actually used it when connecting. If a user has
			 * connected with the permission to ignore passwords (b_virtualserver_join_ignore_password) and the password,
			 * was not entered, it will not be available.
			 * getServerConnectInfo returns 0 on success, 1 on error or if current server tab is disconnected. */
			char host[SERVERINFO_BUFSIZE];
			/*char password[SERVERINFO_BUFSIZE];*/
			char* password = NULL;  /* Don't receive server password */
			unsigned short port;
			if(!ts3Functions.getServerConnectInfo(serverConnectionHandlerID, host, &port, password, SERVERINFO_BUFSIZE)) {
				char msg[SERVERINFO_BUFSIZE];
				snprintf(msg, sizeof(msg), "Server Connect Info: %s:%d", host, port);
				ts3Functions.printMessageToCurrentTab(msg);
			} else {
				ts3Functions.printMessageToCurrentTab("No server connect info available.");
			}
			break;
		}
		case CMD_CHANNELINFO: {  /* /test channelinfo */
			printf("[DEBUG] processing a CMD_CHANNELINFO command\n");
			/* Query channel path and password of current server tab.
			 * The password parameter can be NULL if the plugin does not want to receive the channel password.
			 * Note: Channel password is only available if the user has actually used it when entering the channel. If a user has
			 * entered a channel with the permission to ignore passwords (b_channel_join_ignore_password) and the password,
			 * was not entered, it will not be available.
			 * getChannelConnectInfo returns 0 on success, 1 on error or if current server tab is disconnected. */
			char path[CHANNELINFO_BUFSIZE];
			/*char password[CHANNELINFO_BUFSIZE];*/
			char* password = NULL;  /* Don't receive channel password */

			/* Get own clientID and channelID */
			//FIXME si rompe qua in getClientID oppure in getChannelOfClient (può essere colpa di 'serverConnectionHandlerID'?)
			anyID myID;
			uint64 myChannelID;
			if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
				ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
				break;
			}
			/* Get own channel ID */
			if(ts3Functions.getChannelOfClient(serverConnectionHandlerID, myID, &myChannelID) != ERROR_ok) {
				ts3Functions.logMessage("Error querying channel ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
				break;
			}

			/* Get channel connect info of own channel */
			if(!ts3Functions.getChannelConnectInfo(serverConnectionHandlerID, myChannelID, path, password, CHANNELINFO_BUFSIZE)) {
				char msg[CHANNELINFO_BUFSIZE];
				snprintf(msg, sizeof(msg), "Channel Connect Info: %s", path);
				ts3Functions.printMessageToCurrentTab(msg);
				ts3Functions.requestSendChannelTextMsg(
					serverConnectionHandlerID, 
					msg,
					myChannelID,
					NULL
				);
			} else {
				ts3Functions.printMessageToCurrentTab("No channel connect info available.");
				ts3Functions.requestSendChannelTextMsg(
					serverConnectionHandlerID, 
					"No channel connect info available.",
					myChannelID,
					NULL
				);
			}
			break;
		}
		case CMD_AVATAR: {  /* /test avatar <clientID> */
			char avatarPath[PATH_BUFSIZE];
			anyID clientID = (anyID)atoi(param1);
			unsigned int error;

			memset(avatarPath, 0, PATH_BUFSIZE);
			error = ts3Functions.getAvatar(serverConnectionHandlerID, clientID, avatarPath, PATH_BUFSIZE);
			if(error == ERROR_ok) {  /* ERROR_ok means the client has an avatar set. */
				if(strlen(avatarPath)) {  /* Avatar path contains the full path to the avatar image in the TS3Client cache directory */
					printf("Avatar path: %s\n", avatarPath);
				} else { /* Empty avatar path means the client has an avatar but the image has not yet been cached. The TeamSpeak
						  * client will automatically start the download and call onAvatarUpdated when done */
					printf("Avatar not yet downloaded, waiting for onAvatarUpdated...\n");
				}
			} else if(error == ERROR_database_empty_result) {  /* Not an error, the client simply has no avatar set */
				printf("Client has no avatar\n");
			} else { /* Other error occured (invalid server connection handler ID, invalid client ID, file io error etc) */
				printf("Error getting avatar: %d\n", error);
			}
			break;
		}
		case CMD_ENABLEMENU:  /* /test enablemenu <menuID> <0|1> */
			if(param1) {
				int menuID = atoi(param1);
				int enable = param2 ? atoi(param2) : 0;
				ts3Functions.setPluginMenuEnabled(pluginID, menuID, enable);
			} else {
				ts3Functions.printMessageToCurrentTab("Usage is: /test enablemenu <menuID> <0|1>");
			}
			break;
		case CMD_SUBSCRIBE:  /* /test subscribe <channelID> */
			if(param1) {
				char returnCode[RETURNCODE_BUFSIZE];
				uint64 channelIDArray[2];
				channelIDArray[0] = (uint64)atoi(param1);
				channelIDArray[1] = 0;
				ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
				if(ts3Functions.requestChannelSubscribe(serverConnectionHandlerID, channelIDArray, returnCode) != ERROR_ok) {
					ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
				}
			}
			break;
		case CMD_UNSUBSCRIBE:  /* /test unsubscribe <channelID> */
			if(param1) {
				char returnCode[RETURNCODE_BUFSIZE];
				uint64 channelIDArray[2];
				channelIDArray[0] = (uint64)atoi(param1);
				channelIDArray[1] = 0;
				ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
				if(ts3Functions.requestChannelUnsubscribe(serverConnectionHandlerID, channelIDArray, NULL) != ERROR_ok) {
					ts3Functions.logMessage("Error unsubscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
				}
			}
			break;
		case CMD_SUBSCRIBEALL: {  /* /test subscribeall */
			char returnCode[RETURNCODE_BUFSIZE];
			ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
			if(ts3Functions.requestChannelSubscribeAll(serverConnectionHandlerID, returnCode) != ERROR_ok) {
				ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
			}
			break;
		}
		case CMD_UNSUBSCRIBEALL: {  /* /test unsubscribeall */
			char returnCode[RETURNCODE_BUFSIZE];
			ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
			if(ts3Functions.requestChannelUnsubscribeAll(serverConnectionHandlerID, returnCode) != ERROR_ok) {
				ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
			}
			break;
		}
        case CMD_BOOKMARKSLIST: {  /* test bookmarkslist */
            struct PluginBookmarkList* list;
            unsigned int error = ts3Functions.getBookmarkList(&list);
            if (error == ERROR_ok) {
                print_and_free_bookmarks_list(list);
            }
            else {
                ts3Functions.logMessage("Error getting bookmarks list", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            }
            break;
        }
	}

	return 0;  /* Plugin handled command */
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
	currentServerConnectionHandlerID = serverConnectionHandlerID;
    printf("PLUGIN: currentServerConnectionChanged %llu (%llu)\n", (long long unsigned int)serverConnectionHandlerID, (long long unsigned int)ts3Functions.getCurrentServerConnectionHandlerID());
}

/*
 * Implement the following three functions when the plugin should display a line in the server/channel/client info.
 * If any of ts3plugin_infoTitle, ts3plugin_infoData or ts3plugin_freeMemory is missing, the info text will not be displayed.
 */

/* Static title shown in the left column in the info frame */
const char* ts3plugin_infoTitle() {
	return "Test plugin";
}

/*
 * Dynamic content shown in the right column in the info frame. Memory for the data string needs to be allocated in this
 * function. The client will call ts3plugin_freeMemory once done with the string to release the allocated memory again.
 * Check the parameter "type" if you want to implement this feature only for specific item types. Set the parameter
 * "data" to NULL to have the client ignore the info data.
 */
void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data) {
	char* name;

	/* For demonstration purpose, display the name of the currently selected server, channel or client. */
	switch(type) {
		case PLUGIN_SERVER:
			if(ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &name) != ERROR_ok) {
				printf("Error getting virtual server name\n");
				return;
			}
			break;
		case PLUGIN_CHANNEL:
			if(ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, id, CHANNEL_NAME, &name) != ERROR_ok) {
				printf("Error getting channel name\n");
				return;
			}
			break;
		case PLUGIN_CLIENT:
			if(ts3Functions.getClientVariableAsString(serverConnectionHandlerID, (anyID)id, CLIENT_NICKNAME, &name) != ERROR_ok) {
				printf("Error getting client nickname\n");
				return;
			}
			break;
		default:
			printf("Invalid item type: %d\n", type);
			data = NULL;  /* Ignore */
			return;
	}

	*data = (char*)malloc(INFODATA_BUFSIZE * sizeof(char));  /* Must be allocated in the plugin! */
	snprintf(*data, INFODATA_BUFSIZE, "The nickname is [I]\"%s\"[/I]", name);  /* bbCode is supported. HTML is not supported */
	ts3Functions.freeMemory(name);
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

/* Helper function to create a menu item */
static struct PluginMenuItem* createMenuItem(enum PluginMenuType type, int id, const char* text, const char* icon) {
	struct PluginMenuItem* menuItem = (struct PluginMenuItem*)malloc(sizeof(struct PluginMenuItem));
	menuItem->type = type;
	menuItem->id = id;
	_strcpy(menuItem->text, PLUGIN_MENU_BUFSZ, text);
	_strcpy(menuItem->icon, PLUGIN_MENU_BUFSZ, icon);
	return menuItem;
}

/* Some makros to make the code to create menu items a bit more readable */
#define BEGIN_CREATE_MENUS(x) const size_t sz = x + 1; size_t n = 0; *menuItems = (struct PluginMenuItem**)malloc(sizeof(struct PluginMenuItem*) * sz);
#define CREATE_MENU_ITEM(a, b, c, d) (*menuItems)[n++] = createMenuItem(a, b, c, d);
#define END_CREATE_MENUS (*menuItems)[n++] = NULL; assert(n == sz);

/*
 * Menu IDs for this plugin. Pass these IDs when creating a menuitem to the TS3 client. When the menu item is triggered,
 * ts3plugin_onMenuItemEvent will be called passing the menu ID of the triggered menu item.
 * These IDs are freely choosable by the plugin author. It's not really needed to use an enum, it just looks prettier.
 */
enum {
	MENU_ID_CLIENT_1 = 1,
	MENU_ID_CLIENT_2,
	MENU_ID_CHANNEL_1,
	MENU_ID_CHANNEL_2,
	MENU_ID_CHANNEL_3,
	MENU_ID_GLOBAL_1,
	MENU_ID_GLOBAL_2
};

/*
 * Initialize plugin menus.
 * This function is called after ts3plugin_init and ts3plugin_registerPluginID. A pluginID is required for plugin menus to work.
 * Both ts3plugin_registerPluginID and ts3plugin_freeMemory must be implemented to use menus.
 * If plugin menus are not used by a plugin, do not implement this function or return NULL.
 */
void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon) {
	/*
	 * Create the menus
	 * There are three types of menu items:
	 * - PLUGIN_MENU_TYPE_CLIENT:  Client context menu
	 * - PLUGIN_MENU_TYPE_CHANNEL: Channel context menu
	 * - PLUGIN_MENU_TYPE_GLOBAL:  "Plugins" menu in menu bar of main window
	 *
	 * Menu IDs are used to identify the menu item when ts3plugin_onMenuItemEvent is called
	 *
	 * The menu text is required, max length is 128 characters
	 *
	 * The icon is optional, max length is 128 characters. When not using icons, just pass an empty string.
	 * Icons are loaded from a subdirectory in the TeamSpeak client plugins folder. The subdirectory must be named like the
	 * plugin filename, without dll/so/dylib suffix
	 * e.g. for "test_plugin.dll", icon "1.png" is loaded from <TeamSpeak 3 Client install dir>\plugins\test_plugin\1.png
	 */

	BEGIN_CREATE_MENUS(7);  /* IMPORTANT: Number of menu items must be correct! */
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT,  MENU_ID_CLIENT_1,  "Client item 1",  "1.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT,  MENU_ID_CLIENT_2,  "Client item 2",  "2.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_CHANNEL_1, "Channel item 1", "1.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_CHANNEL_2, "Channel item 2", "2.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_CHANNEL_3, "Channel item 3", "3.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL,  MENU_ID_GLOBAL_1,  "Global item 1",  "1.png");
	CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL,  MENU_ID_GLOBAL_2,  "Global item 2",  "2.png");
	END_CREATE_MENUS;  /* Includes an assert checking if the number of menu items matched */

	/*
	 * Specify an optional icon for the plugin. This icon is used for the plugins submenu within context and main menus
	 * If unused, set menuIcon to NULL
	 */
	*menuIcon = (char*)malloc(PLUGIN_MENU_BUFSZ * sizeof(char));
	_strcpy(*menuIcon, PLUGIN_MENU_BUFSZ, "t.png");

	/*
	 * Menus can be enabled or disabled with: ts3Functions.setPluginMenuEnabled(pluginID, menuID, 0|1);
	 * Test it with plugin command: /test enablemenu <menuID> <0|1>
	 * Menus are enabled by default. Please note that shown menus will not automatically enable or disable when calling this function to
	 * ensure Qt menus are not modified by any thread other the UI thread. The enabled or disable state will change the next time a
	 * menu is displayed.
	 */
	/* For example, this would disable MENU_ID_GLOBAL_2: */
	/* ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_GLOBAL_2, 0); */

	/* All memory allocated in this function will be automatically released by the TeamSpeak client later by calling ts3plugin_freeMemory */
}

/* Helper function to create a hotkey */
static struct PluginHotkey* createHotkey(const char* keyword, const char* description) {
	struct PluginHotkey* hotkey = (struct PluginHotkey*)malloc(sizeof(struct PluginHotkey));
	_strcpy(hotkey->keyword, PLUGIN_HOTKEY_BUFSZ, keyword);
	_strcpy(hotkey->description, PLUGIN_HOTKEY_BUFSZ, description);
	return hotkey;
}

/* Some makros to make the code to create hotkeys a bit more readable */
#define BEGIN_CREATE_HOTKEYS(x) const size_t sz = x + 1; size_t n = 0; *hotkeys = (struct PluginHotkey**)malloc(sizeof(struct PluginHotkey*) * sz);
#define CREATE_HOTKEY(a, b) (*hotkeys)[n++] = createHotkey(a, b);
#define END_CREATE_HOTKEYS (*hotkeys)[n++] = NULL; assert(n == sz);

/*
 * Initialize plugin hotkeys. If your plugin does not use this feature, this function can be omitted.
 * Hotkeys require ts3plugin_registerPluginID and ts3plugin_freeMemory to be implemented.
 * This function is automatically called by the client after ts3plugin_init.
 */
void ts3plugin_initHotkeys(struct PluginHotkey*** hotkeys) {
	/* Register hotkeys giving a keyword and a description.
	 * The keyword will be later passed to ts3plugin_onHotkeyEvent to identify which hotkey was triggered.
	 * The description is shown in the clients hotkey dialog. */
	BEGIN_CREATE_HOTKEYS(3);  /* Create 3 hotkeys. Size must be correct for allocating memory. */
	CREATE_HOTKEY("keyword_1", "Test hotkey 1");
	CREATE_HOTKEY("keyword_2", "Test hotkey 2");
	CREATE_HOTKEY("keyword_3", "Test hotkey 3");
	END_CREATE_HOTKEYS;

	/* The client will call ts3plugin_freeMemory to release all allocated memory */
}

/************************** TeamSpeak callbacks ***************************/
/*
 * Following functions are optional, feel free to remove unused callbacks.
 * See the clientlib documentation for details on each function.
 */

/* Clientlib */

uint8_t TESTSRCBUFFER[] = {126,125,125,123,124,124,123,123,123,122,123,121,121,123,121,123,124,120,118,119,120,122,121,120,120,122,122,122,123,123,126,128,125,126,126,127,127,125,125,127,128,127,127,127,127,129,129,129,132,134,135,134,132,133,135,134,133,135,135,129,122,122,125,128,126,122,124,130,131,131,130,132,138,137,132,131,134,134,130,126,127,131,132,127,124,128,132,131,131,134,140,141,137,132,130,132,133,134,135,131,120,115,118,122,123,121,119,126,133,134,133,133,136,136,134,132,130,129,126,121,119,121,125,125,123,125,128,129,132,133,133,136,135,130,124,122,125,131,131,118,105,108,115,118,116,116,122,130,130,127,128,132,134,131,124,121,122,119,115,115,118,124,127,125,126,130,132,131,131,133,135,134,130,124,124,126,129,134,127,113,112,118,122,122,122,126,137,141,136,135,140,143,140,132,129,132,128,121,120,126,134,136,133,135,142,142,137,135,137,139,138,132,127,129,131,133,136,126,111,114,122,124,124,121,123,133,135,129,131,136,137,133,124,121,124,119,110,113,123,129,130,128,129,135,136,129,127,133,136,133,127,122,126,129,132,139,129,112,114,121,121,121,121,126,137,139,133,133,138,139,132,125,124,126,120,113,117,126,131,132,131,132,136,134,127,126,131,133,132,130,126,128,128,131,142,133,111,110,122,126,123,120,127,139,141,135,133,139,142,133,125,123,128,126,118,119,128,134,135,133,135,137,135,127,124,128,132,130,127,125,124,124,127,138,139,111,98,114,124,119,114,118,132,142,134,127,136,141,130,118,115,120,122,115,112,124,134,132,131,136,137,131,123,119,122,127,127,129,131,128,124,126,132,144,138,110,103,120,124,117,117,127,139,144,137,133,140,142,130,118,118,125,128,122,121,132,139,136,136,141,140,130,121,121,126,131,132,133,136,135,130,129,136,145,144,122,107,117,128,124,119,127,140,148,143,132,133,138,132,120,115,119,127,127,124,128,136,139,135,133,136,132,120,114,119,125,128,131,132,134,135,131,131,135,140,139,120,103,110,124,124,120,128,138,143,140,132,130,133,127,115,111,116,124,123,120,124,133,136,131,127,130,132,121,109,110,121,126,125,126,130,132,131,127,126,130,136,128,104,99,116,125,122,122,130,140,144,136,126,128,133,126,115,114,124,132,129,126,132,141,142,133,129,134,137,123,112,118,129,133,131,132,137,140,136,131,130,134,139,129,106,101,117,126,122,123,131,139,141,132,126,129,132,124,112,112,123,128,124,124,132,139,137,132,131,134,136,127,116,117,127,133,132,134,138,140,138,130,129,133,136,135,121,106,110,122,124,122,129,138,142,137,130,127,129,126,117,115,121,127,128,125,129,139,141,136,133,132,133,132,123,113,118,129,132,131,134,139,141,138,132,132,134,135,135,124,108,111,125,128,126,130,137,141,139,132,130,132,129,119,114,120,128,128,125,129,136,136,132,130,130,129,127,123,117,114,119,124,127,129,132,136,134,129,128,128,125,127,133,122,107,112,123,125,125,130,135,140,138,131,131,136,131,121,119,124,128,127,125,131,137,134,130,130,134,135,130,126,121,118,119,121,125,132,136,134,131,131,131,128,127,128,134,127,108,107,119,123,122,124,130,138,138,131,130,135,133,124,121,124,127,127,123,129,138,140,137,136,138,140,136,133,134,129,121,119,124,131,135,136,136,138,136,129,125,126,128,132,124,107,105,114,117,116,121,129,138,137,131,131,133,130,125,121,121,125,125,121,124,132,135,135,135,136,138,137,132,132,131,122,117,120,125,129,132,135,136,135,131,127,127,127,133,133,115,104,110,114,115,117,122,131,135,132,130,132,132,128,123,121,122,124,119,118,124,127,126,127,129,134,135,129,126,129,125,115,113,118,122,123,125,130,135,133,128,128,131,131,136,135,121,114,118,119,119,120,124,132,139,137,136,139,142,139,133,129,133,134,127,124,128,130,131,131,133,138,141,139,138,140,139,133,129,129,130,131,131,132,136,137,134,132,134,135,136,138,131,123,124,124,122,122,122,125,129,129,129,129,131,132,130,127,128,131,129,125,125,124,123,122,121,122,126,127,126,126,127,127,125,124,125,129,130,127,126,125,126,126,124,125,126,128,130,126,122,124,126,126,124,122,122,127,127,126,126,127,129,130,130,131,134,134,131,130,128,128,129,126,123,124,126,126,124,123,126,127,124,123,126,128,127,124,125,129,129,125,123,125,127,128,129,129,126,124,124,126,126,124,122,125,127,126,125,126,129,130,129,130,133,134,134,133,132,132,132,131,130,131,131,131,131,130,130,133};

void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
    /* Some example code following to show how to use the information query functions. */

    if(newStatus == STATUS_CONNECTION_ESTABLISHED) {  /* connection established and we have client and channels available */


		char* s;
        char msg[1024];
        anyID myID;
        uint64* ids;
        size_t i;
		unsigned int error;

        /* Print clientlib version */
        if(ts3Functions.getClientLibVersion(&s) == ERROR_ok) {
            printf("PLUGIN: Client lib version: %s\n", s);
            ts3Functions.freeMemory(s);  /* Release string */
        } else {
            ts3Functions.logMessage("Error querying client lib version", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }

		/* Write plugin name and version to log */
        snprintf(msg, sizeof(msg), "Plugin %s, Version %s, Author: %s", ts3plugin_name(), ts3plugin_version(), ts3plugin_author());
        ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin", serverConnectionHandlerID);

        /* Print virtual server name */
        if((error = ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &s)) != ERROR_ok) {
			if(error != ERROR_not_connected) {  /* Don't spam error in this case (failed to connect) */
				ts3Functions.logMessage("Error querying server name", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
			}
            return;
        }
        printf("PLUGIN: Server name: %s\n", s);
        ts3Functions.freeMemory(s);

        /* Print virtual server welcome message */
        if(ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_WELCOMEMESSAGE, &s) != ERROR_ok) {
            ts3Functions.logMessage("Error querying server welcome message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        printf("PLUGIN: Server welcome message: %s\n", s);
        ts3Functions.freeMemory(s);  /* Release string */

        /* Print own client ID and nickname on this server */
        if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
            ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        if(ts3Functions.getClientSelfVariableAsString(serverConnectionHandlerID, CLIENT_NICKNAME, &s) != ERROR_ok) {
            ts3Functions.logMessage("Error querying client nickname", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        printf("PLUGIN: My client ID = %d, nickname = %s\n", myID, s);
        ts3Functions.freeMemory(s);

        /* Print list of all channels on this server */
        if(ts3Functions.getChannelList(serverConnectionHandlerID, &ids) != ERROR_ok) {
            ts3Functions.logMessage("Error getting channel list", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        printf("PLUGIN: Available channels:\n");
        for(i=0; ids[i]; i++) {
            /* Query channel name */
            if(ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, ids[i], CHANNEL_NAME, &s) != ERROR_ok) {
                ts3Functions.logMessage("Error querying channel name", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                return;
            }
            printf("PLUGIN: Channel ID = %llu, name = %s\n", (long long unsigned int)ids[i], s);
            ts3Functions.freeMemory(s);
        }
        ts3Functions.freeMemory(ids);  /* Release array */

        /* Print list of existing server connection handlers */
        printf("PLUGIN: Existing server connection handlers:\n");
        if(ts3Functions.getServerConnectionHandlerList(&ids) != ERROR_ok) {
            ts3Functions.logMessage("Error getting server list", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        for(i=0; ids[i]; i++) {
            if((error = ts3Functions.getServerVariableAsString(ids[i], VIRTUALSERVER_NAME, &s)) != ERROR_ok) {
				if(error != ERROR_not_connected) {  /* Don't spam error in this case (failed to connect) */
					ts3Functions.logMessage("Error querying server name", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
				}
                continue;
            }
            printf("- %llu - %s\n", (long long unsigned int)ids[i], s);
            ts3Functions.freeMemory(s);
        }
        ts3Functions.freeMemory(ids);

		// // /* Open capture device we created earlier */
		if(ts3Functions.openCaptureDevice(serverConnectionHandlerID, "custom", devID) != ERROR_ok) {
			printf("Error opening capture device\n");
			exit(1);
		}

		// //open the device for playback
		printf("======================= DEBUG =======================\n");
		printf("======================= ===== =======================\n");
		char* defaultMode;

		if(ts3Functions.getDefaultPlayBackMode(&defaultMode) == ERROR_ok) {
			char*** array;

			if(ts3Functions.getPlaybackDeviceList(defaultMode, &array) == ERROR_ok) {
				for(int i=0; array[i] != NULL; ++i) {
					printf("Playback device name: %s\n", array[i][0]);  /* First element: Device name */
					printf("Playback device ID: %s\n",   array[i][1]);  /* Second element: Device ID */

					/* Free element */
					ts3Functions.freeMemory(array[i][0]);
					ts3Functions.freeMemory(array[i][1]);
					ts3Functions.freeMemory(array[i]);
				}
				ts3Functions.freeMemory(array);  /* Free complete array */
			} else {
				printf("Error getting playback device list\n");
			}
		} else {
			printf("Error getting default playback mode\n");
		}
		char** array;

		if(ts3Functions.getPlaybackModeList(&array) == ERROR_ok) {
			for(int i=0; array[i] != NULL; ++i) {
				printf("Mode: %s\n", array[i]);
				ts3Functions.freeMemory(array[i]);  // Free C-string
			}
			ts3Functions.freeMemory(array);  // Free the array
		}

		if (ts3Functions.openPlaybackDevice(serverConnectionHandlerID, "custom", devID) != ERROR_ok){
			printf("Error opening playback device\n");
			exit(1);
		}

		//0 means the semaphore is shared between threads
		//1 is the initial value of the semaphore
		sem_init(&c_pb_sem, 0, 1);

		pthread_t t1;
		pthread_t t2;
		// pthread_create(&t1, NULL, main_loop_play, NULL);
		pthread_create(&t1, NULL, main_loop_play, ((void *)(&ts3Functions))); //TODO thread should be global, stop when disconneting
		pthread_create(&t2, NULL, main_loop_acquire, ((void *)(&ts3Functions))); //TODO thread should be global, stop when disconneting
		

		//INIT MY AT COMMANDS LIBRARY
		at_init(NULL, NULL);
    }
}



void ts3plugin_onNewChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID) {
}

void ts3plugin_onNewChannelCreatedEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
}

void ts3plugin_onDelChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
}

void ts3plugin_onChannelMoveEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 newChannelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
}

void ts3plugin_onUpdateChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onUpdateChannelEditedEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
}

void ts3plugin_onUpdateClientEvent(uint64 serverConnectionHandlerID, anyID clientID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {
}

void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage) {
}

void ts3plugin_onClientMoveSubscriptionEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility) {
}

void ts3plugin_onClientMoveTimeoutEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* timeoutMessage) {
}

void ts3plugin_onClientMoveMovedEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID moverID, const char* moverName, const char* moverUniqueIdentifier, const char* moveMessage) {
}

void ts3plugin_onClientKickFromChannelEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {
}

void ts3plugin_onClientKickFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {
}

void ts3plugin_onClientIDsEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, anyID clientID, const char* clientName) {
}

void ts3plugin_onClientIDsFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onServerEditedEvent(uint64 serverConnectionHandlerID, anyID editerID, const char* editerName, const char* editerUniqueIdentifier) {
}

void ts3plugin_onServerUpdatedEvent(uint64 serverConnectionHandlerID) {
}

int ts3plugin_onServerErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, const char* extraMessage) {
	printf("PLUGIN: onServerErrorEvent %llu %s %d %s\n", (long long unsigned int)serverConnectionHandlerID, errorMessage, error, (returnCode ? returnCode : ""));
	if(returnCode) {
		/* A plugin could now check the returnCode with previously (when calling a function) remembered returnCodes and react accordingly */
		/* In case of using a a plugin return code, the plugin can return:
		 * 0: Client will continue handling this error (print to chat tab)
		 * 1: Client will ignore this error, the plugin announces it has handled it */
		return 1;
	}
	return 0;  /* If no plugin return code was used, the return value of this function is ignored */
}

void ts3plugin_onServerStopEvent(uint64 serverConnectionHandlerID, const char* shutdownMessage) {
}

// si è rotto a 30, 34 caratteri

int ts3plugin_onTextMessageEvent(uint64 serverConnectionHandlerID, anyID targetMode, anyID toID, anyID fromID, const char* fromName, const char* fromUniqueIdentifier, const char* message, int ffIgnored) {
    printf("[DEBUG] ts3plugin_onTextMessageEvent!\n");
	printf("PLUGIN: onTextMessageEvent %llu %d %d %s %s %d\n", (long long unsigned int)serverConnectionHandlerID, targetMode, fromID, fromName, message, ffIgnored);

	/* Friend/Foe manager has ignored the message, so ignore here as well. */
	if(ffIgnored) {
		return 0; /* Client will ignore the message anyways, so return value here doesn't matter */
	}

	//check if user is enabled

	#if 1

	if (strncmp(message, ts3plugin_commandKeyword(), strlen(ts3plugin_commandKeyword())) == 0){
		printf("[DEBUG] got a command!!!!\n");
		anyID myID;
		if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
			ts3Functions.logMessage("Error querying own client id", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
			return 1;
		}

		//assume every command is an at command
		char* output = NULL;
		int at_message_len = strlen(message) - (strlen(ts3plugin_commandKeyword()) + 1);
		char at_message[at_message_len+1];
		strncpy(at_message, message + (strlen(ts3plugin_commandKeyword()) + 1), at_message_len);

		at_message[at_message_len] = '\0';
		int at_output = at_process_command(at_message, &output);
		// char* mymessage = "!test phonebook read 1 9\0";
		// int at_output = at_process_command(mymessage, &output);
		printf("[DEBUG] output of at_command: %s\n", output);
		if (output == NULL) //we did not handle the command
			return ts3plugin_processCommand(serverConnectionHandlerID, message);
		else {
			printf("[DEBUG] I should reply: %s\n", output);

			uint64 myChannelID;
			/* Get own channel ID */
			if(ts3Functions.getChannelOfClient(serverConnectionHandlerID, myID, &myChannelID) != ERROR_ok) {
				ts3Functions.logMessage("Error querying channel ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
			}
			// printf("[DEBUG] got channel id: %li and serverConnHandlID %li\n", myChannelID, serverConnectionHandlerID);
			
			ts3Functions.requestSendChannelTextMsg(
				serverConnectionHandlerID, 
				output,
				myChannelID,
				NULL
			);
			//TODO scommenta
			// free(output);
			printf("[DEUBG] at_output is %i\n", at_output);
			return at_output;
		}
		
	} else {
		printf("[DEBUG] message/keyword/kw/cmp: %s/%s/%i\n", message, ts3plugin_commandKeyword(), strlen(ts3plugin_commandKeyword()), strncmp(message, ts3plugin_commandKeyword(), strlen(ts3plugin_commandKeyword())));
	}

	// if (!strncmp(message, "/tsgsm", 6)){
	// 	return 1;
	// }

	// char command[(sizeof(message)-7)+1];
	// strcpy(command, message[7]);

	#endif

#if 0
	{
		/* Example code: Autoreply to sender */
		/* Disabled because quite annoying, but should give you some ideas what is possible here */
		/* Careful, when two clients use this, they will get banned quickly... */
		anyID myID;
		if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
			ts3Functions.logMessage("Error querying own client id", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
			return 0;
		}
		if(fromID != myID) {  /* Don't reply when source is own client */
			if(ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Text message back!", fromID, NULL) != ERROR_ok) {
				ts3Functions.logMessage("Error requesting send text message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
			}
		}
	}
#endif

    return 1;  /* 0 = handle normally, 1 = client will ignore the text message */
}

void ts3plugin_onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID) {
	/* Demonstrate usage of getClientDisplayName */
	char name[512];
	if(ts3Functions.getClientDisplayName(serverConnectionHandlerID, clientID, name, 512) == ERROR_ok) {
		if(status == STATUS_TALKING) {
			printf("--> %s starts talking\n", name);
		} else {
			printf("--> %s stops talking\n", name);
		}
	}
}

void ts3plugin_onConnectionInfoEvent(uint64 serverConnectionHandlerID, anyID clientID) {
}

void ts3plugin_onServerConnectionInfoEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onChannelSubscribeEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onChannelSubscribeFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onChannelUnsubscribeEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onChannelUnsubscribeFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onChannelDescriptionUpdateEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onChannelPasswordChangedEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onPlaybackShutdownCompleteEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onSoundDeviceListChangedEvent(const char* modeID, int playOrCap) {
}

void ts3plugin_onEditPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels) {
	// printf("we got voice! %i samples\n", sampleCount);
	// send_voice(samples, sampleCount, channels);

	// for (int i = 0; i < sampleCount; i++){
	// 	printf("%i,", samples[i]);
	// }
	// printf("\n");
	/*for (int i = 0; i < sampleCount; i++)
		printf("%#06x", samples[i]);*/
	// for(int i = 0; i < sampleCount; i++){
	// 	audio_buffer_to_ds[data_in_buffer+i] = samples[i];
	// }

	/***
	memcpy(&(audio_buffer_to_ds[data_in_buffer]), samples, sampleCount*sizeof(short));
	data_in_buffer += sampleCount;

	if (data_in_buffer >= MIN_BUFFER){
		printf("\nusing %i channels\n", channels);
		int res = send_voice(audio_buffer_to_ds, data_in_buffer, channels);
		// int res = send_voice(samples, sampleCount, channels);
		printf("We got %i sample counts\n", data_in_buffer);

		memset(audio_buffer_to_ds, 0, data_in_buffer);
		data_in_buffer = 0;
	}
	***/


	/*if (res == EXIT_FAILURE)
		printf("FAILURE ❗❗❗\n");
	else if (res == EXIT_SUCCESS){
		printf("SUCCESS 🏄🏄🏄\n");
	}*/

	// printf("[Original]\n");

	// for (size_t i = 0; i < 70; i++){
	// 	printf("0x%x ", samples[i]);
	// }
	// printf("\n");
	// printf("[casted]\n");
	// for (size_t i = 0; i < 70; i++){
	// 	printf("0x%x ", (uint8_t)samples[i]);
	// }
	// printf("\n");

}



void ts3plugin_onEditPostProcessVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask) {
}

void ts3plugin_onEditMixedPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask) {
}

void ts3plugin_onEditCapturedVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, int* edited) {
	// printf("something something ts3plugin_onEditCapturedVoiceDataEvent\n");
}

void ts3plugin_onCustom3dRolloffCalculationClientEvent(uint64 serverConnectionHandlerID, anyID clientID, float distance, float* volume) {
}

void ts3plugin_onCustom3dRolloffCalculationWaveEvent(uint64 serverConnectionHandlerID, uint64 waveHandle, float distance, float* volume) {
}

void ts3plugin_onUserLoggingMessageEvent(const char* logMessage, int logLevel, const char* logChannel, uint64 logID, const char* logTime, const char* completeLogString) {
}

/* Clientlib rare */

void ts3plugin_onClientBanFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, uint64 time, const char* kickMessage) {
}

int ts3plugin_onClientPokeEvent(uint64 serverConnectionHandlerID, anyID fromClientID, const char* pokerName, const char* pokerUniqueIdentity, const char* message, int ffIgnored) {
    anyID myID;

    printf("PLUGIN onClientPokeEvent: %llu %d %s %s %d\n", (long long unsigned int)serverConnectionHandlerID, fromClientID, pokerName, message, ffIgnored);

	/* Check if the Friend/Foe manager has already blocked this poke */
	if(ffIgnored) {
		return 0;  /* Client will block anyways, doesn't matter what we return */
	}

    /* Example code: Send text message back to poking client */
    if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {  /* Get own client ID */
        ts3Functions.logMessage("Error querying own client id", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
        return 0;
    }
    if(fromClientID != myID) {  /* Don't reply when source is own client */
        if(ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Received your poke!", fromClientID, NULL) != ERROR_ok) {
            ts3Functions.logMessage("Error requesting send text message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
        }
    }

    return 0;  /* 0 = handle normally, 1 = client will ignore the poke */
}

void ts3plugin_onClientSelfVariableUpdateEvent(uint64 serverConnectionHandlerID, int flag, const char* oldValue, const char* newValue) {
}

void ts3plugin_onFileListEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* path, const char* name, uint64 size, uint64 datetime, int type, uint64 incompletesize, const char* returnCode) {
}

void ts3plugin_onFileListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* path) {
}

void ts3plugin_onFileInfoEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* name, uint64 size, uint64 datetime) {
}

void ts3plugin_onServerGroupListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, const char* name, int type, int iconID, int saveDB) {
}

void ts3plugin_onServerGroupListFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onServerGroupByClientIDEvent(uint64 serverConnectionHandlerID, const char* name, uint64 serverGroupList, uint64 clientDatabaseID) {
}

void ts3plugin_onServerGroupPermListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onServerGroupPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID) {
}

void ts3plugin_onServerGroupClientListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, uint64 clientDatabaseID, const char* clientNameIdentifier, const char* clientUniqueID) {
}

void ts3plugin_onChannelGroupListEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, const char* name, int type, int iconID, int saveDB) {
}

void ts3plugin_onChannelGroupListFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onChannelGroupPermListEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onChannelGroupPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID) {
}

void ts3plugin_onChannelPermListEvent(uint64 serverConnectionHandlerID, uint64 channelID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onChannelPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID) {
}

void ts3plugin_onClientPermListEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onClientPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID) {
}

void ts3plugin_onChannelClientPermListEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 clientDatabaseID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onChannelClientPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 clientDatabaseID) {
}

void ts3plugin_onClientChannelGroupChangedEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, uint64 channelID, anyID clientID, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity) {
}

int ts3plugin_onServerPermissionErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, unsigned int failedPermissionID) {
	return 0;  /* See onServerErrorEvent for return code description */
}

void ts3plugin_onPermissionListGroupEndIDEvent(uint64 serverConnectionHandlerID, unsigned int groupEndID) {
}

void ts3plugin_onPermissionListEvent(uint64 serverConnectionHandlerID, unsigned int permissionID, const char* permissionName, const char* permissionDescription) {
}

void ts3plugin_onPermissionListFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onPermissionOverviewEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID, uint64 channelID, int overviewType, uint64 overviewID1, uint64 overviewID2, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {
}

void ts3plugin_onPermissionOverviewFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onServerGroupClientAddedEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientName, const char* clientUniqueIdentity, uint64 serverGroupID, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity) {
}

void ts3plugin_onServerGroupClientDeletedEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientName, const char* clientUniqueIdentity, uint64 serverGroupID, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity) {
}

void ts3plugin_onClientNeededPermissionsEvent(uint64 serverConnectionHandlerID, unsigned int permissionID, int permissionValue) {
}

void ts3plugin_onClientNeededPermissionsFinishedEvent(uint64 serverConnectionHandlerID) {
}

void ts3plugin_onFileTransferStatusEvent(anyID transferID, unsigned int status, const char* statusMessage, uint64 remotefileSize, uint64 serverConnectionHandlerID) {
}

void ts3plugin_onClientChatClosedEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientUniqueIdentity) {
}

void ts3plugin_onClientChatComposingEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientUniqueIdentity) {
}

void ts3plugin_onServerLogEvent(uint64 serverConnectionHandlerID, const char* logMsg) {
}

void ts3plugin_onServerLogFinishedEvent(uint64 serverConnectionHandlerID, uint64 lastPos, uint64 fileSize) {
}

void ts3plugin_onMessageListEvent(uint64 serverConnectionHandlerID, uint64 messageID, const char* fromClientUniqueIdentity, const char* subject, uint64 timestamp, int flagRead) {
}

void ts3plugin_onMessageGetEvent(uint64 serverConnectionHandlerID, uint64 messageID, const char* fromClientUniqueIdentity, const char* subject, const char* message, uint64 timestamp) {
}

void ts3plugin_onClientDBIDfromUIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID) {
}

void ts3plugin_onClientNamefromUIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID, const char* clientNickName) {
}

void ts3plugin_onClientNamefromDBIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID, const char* clientNickName) {
}

void ts3plugin_onComplainListEvent(uint64 serverConnectionHandlerID, uint64 targetClientDatabaseID, const char* targetClientNickName, uint64 fromClientDatabaseID, const char* fromClientNickName, const char* complainReason, uint64 timestamp) {
}

void ts3plugin_onBanListEvent(uint64 serverConnectionHandlerID, uint64 banid, const char* ip, const char* name, const char* uid, const char* mytsid, uint64 creationTime, uint64 durationTime, const char* invokerName,
							  uint64 invokercldbid, const char* invokeruid, const char* reason, int numberOfEnforcements, const char* lastNickName) {
}

void ts3plugin_onClientServerQueryLoginPasswordEvent(uint64 serverConnectionHandlerID, const char* loginPassword) {
}

void ts3plugin_onPluginCommandEvent(uint64 serverConnectionHandlerID, const char* pluginName, const char* pluginCommand, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity) {
	printf("ON PLUGIN COMMAND: %s %s %d %s %s\n", pluginName, pluginCommand, invokerClientID, invokerName, invokerUniqueIdentity);
}

void ts3plugin_onIncomingClientQueryEvent(uint64 serverConnectionHandlerID, const char* commandText) {
}

void ts3plugin_onServerTemporaryPasswordListEvent(uint64 serverConnectionHandlerID, const char* clientNickname, const char* uniqueClientIdentifier, const char* description, const char* password, uint64 timestampStart, uint64 timestampEnd, uint64 targetChannelID, const char* targetChannelPW) {
}

/* Client UI callbacks */

/*
 * Called from client when an avatar image has been downloaded to or deleted from cache.
 * This callback can be called spontaneously or in response to ts3Functions.getAvatar()
 */
void ts3plugin_onAvatarUpdated(uint64 serverConnectionHandlerID, anyID clientID, const char* avatarPath) {
	/* If avatarPath is NULL, the avatar got deleted */
	/* If not NULL, avatarPath contains the path to the avatar file in the TS3Client cache */
	if(avatarPath != NULL) {
		printf("onAvatarUpdated: %llu %d %s\n", (long long unsigned int)serverConnectionHandlerID, clientID, avatarPath);
	} else {
		printf("onAvatarUpdated: %llu %d - deleted\n", (long long unsigned int)serverConnectionHandlerID, clientID);
	}
}

/*
 * Called when a plugin menu item (see ts3plugin_initMenus) is triggered. Optional function, when not using plugin menus, do not implement this.
 *
 * Parameters:
 * - serverConnectionHandlerID: ID of the current server tab
 * - type: Type of the menu (PLUGIN_MENU_TYPE_CHANNEL, PLUGIN_MENU_TYPE_CLIENT or PLUGIN_MENU_TYPE_GLOBAL)
 * - menuItemID: Id used when creating the menu item
 * - selectedItemID: Channel or Client ID in the case of PLUGIN_MENU_TYPE_CHANNEL and PLUGIN_MENU_TYPE_CLIENT. 0 for PLUGIN_MENU_TYPE_GLOBAL.
 */
void ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID) {
	printf("PLUGIN: onMenuItemEvent: serverConnectionHandlerID=%llu, type=%d, menuItemID=%d, selectedItemID=%llu\n", (long long unsigned int)serverConnectionHandlerID, type, menuItemID, (long long unsigned int)selectedItemID);
	switch(type) {
		case PLUGIN_MENU_TYPE_GLOBAL:
			/* Global menu item was triggered. selectedItemID is unused and set to zero. */
			switch(menuItemID) {
				case MENU_ID_GLOBAL_1:
					/* Menu global 1 was triggered */
					break;
				case MENU_ID_GLOBAL_2:
					/* Menu global 2 was triggered */
					break;
				default:
					break;
			}
			break;
		case PLUGIN_MENU_TYPE_CHANNEL:
			/* Channel contextmenu item was triggered. selectedItemID is the channelID of the selected channel */
			switch(menuItemID) {
				case MENU_ID_CHANNEL_1:
					/* Menu channel 1 was triggered */
					break;
				case MENU_ID_CHANNEL_2:
					/* Menu channel 2 was triggered */
					break;
				case MENU_ID_CHANNEL_3:
					/* Menu channel 3 was triggered */
					break;
				default:
					break;
			}
			break;
		case PLUGIN_MENU_TYPE_CLIENT:
			/* Client contextmenu item was triggered. selectedItemID is the clientID of the selected client */
			switch(menuItemID) {
				case MENU_ID_CLIENT_1:
					/* Menu client 1 was triggered */
					break;
				case MENU_ID_CLIENT_2:
					/* Menu client 2 was triggered */
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
}

/* This function is called if a plugin hotkey was pressed. Omit if hotkeys are unused. */
void ts3plugin_onHotkeyEvent(const char* keyword) {
	printf("PLUGIN: Hotkey event: %s\n", keyword);
	/* Identify the hotkey by keyword ("keyword_1", "keyword_2" or "keyword_3" in this example) and handle here... */
}

/* Called when recording a hotkey has finished after calling ts3Functions.requestHotkeyInputDialog */
void ts3plugin_onHotkeyRecordedEvent(const char* keyword, const char* key) {
}

// This function receives your key Identifier you send to notifyKeyEvent and should return
// the friendly device name of the device this hotkey originates from. Used for display in UI.
const char* ts3plugin_keyDeviceName(const char* keyIdentifier) {
	return NULL;
}

// This function translates the given key identifier to a friendly key name for display in the UI
const char* ts3plugin_displayKeyText(const char* keyIdentifier) {
	return NULL;
}

// This is used internally as a prefix for hotkeys so we can store them without collisions.
// Should be unique across plugins.
const char* ts3plugin_keyPrefix() {
	return NULL;
}

/* Called when client custom nickname changed */
void ts3plugin_onClientDisplayNameChanged(uint64 serverConnectionHandlerID, anyID clientID, const char* displayName, const char* uniqueClientIdentifier) {
}
