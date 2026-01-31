#ifndef AT_COMMANDS_H 
#define AT_COMMANDS_H

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h> 
#include <string.h>
#include <stdlib.h>
#include <iconv.h>
#include <errno.h>
#include <semaphore.h>
#include "../plugin.h"

#include "../settings.h"

#define TCP_BUFFER 1024

// Define a struct for generic CRUD operations
typedef struct {
    // char** (*create)();
    union {
        char* (*create_str)();
        char** (*create_arr_str)();
    };
    char* (*read)();
    char* (*update)();
    char* (*del)();
} CrudAPI;


//phonebook apis
// CrudAPI phonebook_api;
char* at_phonebook_create(char* index, char* name, char* number);
char* at_phonebook_read(char* index1, char* index2);
char* at_phonebook_delete(char* index);

//texts apis
// CrudAPI text_api;
char** at_text_create(char* dest, char* text);
char* at_text_read(char* index, char* mode);
char* at_text_delete(char* index, char* flag);

//call apis
char* at_call_make(const char* number);
char* at_call_hang();
char* at_call_wait();
char* at_call_merge();

//settings apis
/* set wether calls are possible - e.g. if my plan is consumption-based, I don't want to make calls, only receive them */
// char* at_can_call(int flag);
/* set wether texts are possible - e.g. if my plan is consumption-based, I don't want to send texts, only receive them */
// char* at_can_text(int flag);
/* set wether answer calls only from numbers in the phonebook */
char* at_answer_phonebook_only(int flag);
//TODO blacklist?

char** at_set_text_mode(); 

//network
char** at_check_network_status();

//AT
int at_send_command(const char* command, char** output);

char* at_get_own_number();

char* at_help();  

void at_init(char* server, int port);

int at_process_command(const char* command, char** output);

void record_noise();

#endif