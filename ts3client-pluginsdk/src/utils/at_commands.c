/*
    Reference for the commands:
    https://cdn-shop.adafruit.com/datasheets/sim800_series_at_command_manual_v1.01.pdf
*/

#include "at_commands.h"


char* start_command = "RNSTART"; //record noise start
char* stop_command = "RNSTOP"; //record noise stop


static struct sockaddr_in server_addr;

char* at_phonebook_create(char* index, char* name, char* number){
    size_t bufsize = strlen(name) + strlen(number) + 50;
    char* result = (char*)malloc(bufsize);
    if (!result) return NULL;
    if (index != NULL) //used to update a contact
        snprintf(result, bufsize, "AT+CPBW=%s,\"%s\",129,\"%s\"", index, number, name);
    else 
        snprintf(result, bufsize, "AT+CPBW=,\"%s\",129,\"%s\"", number, name);
    return result;
}

//TODO estendere read con logica (es. fare una grep del nome/numero che mi interessa)
char* at_phonebook_read(char* index1, char* index2){
    if (index1 == NULL){
        index1 = "1";
    }
    if (index2 == NULL){
        index2 = "250";
    }

    size_t bufsize = strlen("AT+CPBR=") + strlen(index1) + strlen(index2) + 1;
    char* result = malloc((bufsize)*sizeof(char)+1);
    if (!result) return NULL;
    
    snprintf(result, bufsize+1, "AT+CPBR=%s,%s", index1, index2);
    // result[bufsize+1] = '\0';
    return result;
}

//TODO estendere read con logica (es. ricerca della posizione in base al nomee poi eliminazione)
char* at_phonebook_delete(char* index){
    size_t bufsize = 50;
    char* result = (char*)malloc(bufsize);
    snprintf(result, bufsize, "AT+CPBW=%s", index);  // delete by number
    return result;
}

void utf8_to_ucs2_encoder(char* src, char** output){
    //change encoding
    size_t bufsize = strlen(src);
    size_t usc2_len = bufsize * 2; // Worst case
    iconv_t cd = iconv_open("UCS-2BE", "UTF-8");
    if (cd == (iconv_t)-1) {
	/* Initialization failure. */
	if (errno == EINVAL) {
	    ERROR("Conversion from '%s' to '%s' is not supported.", "UCS-2BE", "UTF-8");
	}
	else {
	    ERROR("Initialization failure: %s", strerror (errno));
	}
	// exit ok
	exit (1);
    }
    char* ucs2_str = malloc(usc2_len); memset(ucs2_str, 0, usc2_len);
    char* inptr = src;
    char* outptr = ucs2_str;
    size_t _bufsize = bufsize;
    size_t _usc2_len = usc2_len;
    
    // size_t irr_converted = iconv(cd, &inptr, &_bufsize, &outptr, &_usc2_len);
    iconv(cd, &inptr, &_bufsize, &outptr, &_usc2_len);
    iconv_close(cd);

    // converting to hex string
    size_t written_bytes = usc2_len - _usc2_len; //written bytes
    *output = malloc(written_bytes*2+1); // + null terminator
    for (size_t i = 0; i < written_bytes; i++) {
        sprintf(*output + i * 2, "%02X", (unsigned char)ucs2_str[i]);
    }
    output[written_bytes*2] = '\0';
    free(ucs2_str);
    // *output = hex_str;
}

/*
    // AT+CMGS=<da>[,<toda>],<CR> send sms
    da: destination addess in string format (string should be included in quotation marks)
    toda: type of address (145 or 129)
*/
char** at_text_create(char* dest, char* text){
    char** commands = malloc(sizeof(char*)*3);

    /* 
    * converting to usc2 encoding to support emojis 😍😍😍 
    */
    char *hex_dest, *hex_text; 
    utf8_to_ucs2_encoder(dest, &hex_dest);    
    utf8_to_ucs2_encoder(text, &hex_text);    


    size_t bufsize = strlen(hex_dest) + strlen(hex_text) + 20;
    char* command1 = malloc(bufsize);
    char* command2 = malloc(bufsize);

    snprintf(command1, bufsize, "AT+CMGS=\"%s\"\r", hex_dest);
    snprintf(command2, bufsize, "%s%c", hex_text, 0x1A);

    free(hex_dest);
    free(hex_text);
    
    commands[0] = command1; commands[1] = command2; commands[2] = NULL;

    return commands;
}

/*
    AT+CMGL=<stat> list sms
    stats:
        REC UNREAD: received unread messages
        REC READ: received read messages
        STO UNSENT: stored unsent messages
        STO SENT: stored sent messages
        ALL: all
    AT+CMGR=<index> read sms
*/
char* at_text_read(char* index, char* mode){
     
    size_t bufsize = 50;
    char* result = malloc(bufsize);
    if (index != NULL)
        snprintf(result, bufsize, "AT+CMGR=%s", index);
    else {
        if(strcmp(mode, "sent") == 0){
            snprintf(result, bufsize, "AT+CMGL=\"%s\"", "STO SENT");
        } else if(strcmp(mode, "unread") == 0){
            snprintf(result, bufsize, "AT+CMGL=\"%s\"", "REC UNREAD");
        } else if(strcmp(mode, "read") == 0){
            snprintf(result, bufsize, "AT+CMGL=\"%s\"", "REC READ");
        }
    }
    return result;

} 

/*
    AT+CMDGD=<index>,<flag> delete sms; 
    flags: 
        0: delete <index>; 
        1: delete all read messages; 
        2: delete all read and sent; 
        3: delete read, sent and saved (unsent) messages; 
        4: delete all messages (even unread)
*/ 
char* at_text_delete(char* index, char* flag){
    
    int delflag = 0;
    if (flag != NULL && strcmp(flag, "read") == 0){
        delflag = 1;
    }
    if (flag != NULL && strcmp(flag, "all") == 0){
        delflag = 4;
    }

    size_t maxlen = 25;
    char* result = malloc(maxlen);
    snprintf(result, maxlen, "AT+CMGD=%s,%i", index, delflag);
            
    return result;
} 

char* at_call_make(const char* number){
    char* buffer = malloc(40);  // allocate memory on the heap
    if (buffer != NULL) {
        snprintf(buffer, 40, "ATD%s;\n", number);
    }
    return buffer;
    
}

char* at_call_hang(){
    char* buffer = malloc(5);
    strcpy(buffer, "ATH\n");
    return buffer;
}

// char* at_call_accept(){
//     char* buffer = malloc(5);
//     strcpy(buffer, "ATA\n");
//     return buffer;
// }

// char* at_call_wait(){
//     char* buffer = malloc(15);
//     strcpy(buffer, "AT+CCWA=1,1,1\n");

// }

// char* at_call_merge(){
//     return NULL;
//     // AT+CHLD=2
// }

// char* at_can_call(int flag){
//     return NULL;
// }

// char* at_can_text(int flag){
//     return NULL;
// }

// TODO se esp32 viene spento, si perde l'impostazione; va salvata dal server di ts e esp32 all'avvio deve chiedere le impostazioni al server
char* at_answer_phonebook_only(int flag){
    char* buffer = malloc(11);
    snprintf(buffer, 11, "PB_ONLY=%i\n", flag);
    return buffer;
}

char* at_get_own_number(){
    char* buffer = malloc(9);
    strcpy(buffer, "AT+CNUM\n");
    return buffer;
}

char** at_set_text_mode(int UCS2){
    char* textmode = "AT+CMGF=1";
    char* encoding;
    char* command1;
    char* command2;
    if (UCS2){
        encoding = "AT+CSCS=\"UCS2\"";
    // snprintf(command1, strlen(textmode) + 1, "%s", textmode);
    // snprintf(command2, strlen(encoding) + 1, "%s", encoding);
    } else {
        encoding = "AT+CSCS=\"GSM\"";
    } 
    command1 = malloc(strlen(textmode)+1);
    command2 = malloc(strlen(encoding)+1);
    strcpy(command1, textmode);
    strcpy(command2, encoding);
    char** commands = malloc(sizeof(char*)*3);
    commands[0] = command1; commands[1] = command2; commands[2] = NULL;
    return commands;
} 

/*
    Execution Command: AT+CSQ
    Response: +CSQ: <rssi>,<ber>
    Parameters
        <rssi>
            0 -115 dBm or less
            1 -111 dBm
            2...30 -110... -54 dBm
            31 -52 dBm or greater
            99 not known or not detectable
        <ber> (in percent):
            0...7 As RXQUAL values in the table in GSM 05.08 [20]
                  subclause 7.2.4
            99 Not known or not detectable
    
    Read Command: AT+CREG?  
    Response: TA returns the status of result code presentation and an integer <stat>
              which shows whether the network has currently indicated the registration
              of the ME. Location information elements <lac> and <ci> are returned
              only when <n>=2 and ME is registered in the network.
              +CREG: <n>,<stat>[,<lac>,<ci>]

    Read Command: AT+COPS?
    Response: TA returns the current mode and the currently selected operator. If no
              operator is selected, <format> and <oper> are omitted.
              +COPS: <mode>[,<format>, <oper>]
*/
char** at_check_network_status(){
// AT+CSQ        --> Check signal quality
// AT+CREG?      --> Check network registration status
// AT+COPS?      --> Check current operator
    char* csq = "AT+CSQ";
    char* creg = "AT+CREG?";
    char* cops = "AT+COPS?";

    char* command1 = malloc(strlen(csq)+1);
    char* command2 = malloc(strlen(creg)+1);
    char* command3 = malloc(strlen(cops)+1);

    strcpy(command1, csq);
    strcpy(command2, creg);
    strcpy(command3, cops);
    // snprintf(command1, strlen(csq)+1, "%s", csq);
    // snprintf(command2, strlen(creg)+1, "%s", creg);
    // snprintf(command3, strlen(cops)+1, "%s", cops);

    char** commands = malloc(sizeof(char*)*4);
    commands[0] = command1; commands[1] = command2; commands[2] = command3; commands[3] = NULL;
    
    return commands;
}

static CrudAPI phonebook_api = {
    .create_str = at_phonebook_create,
    .read = at_phonebook_read,
    .update = NULL,
    .del = at_phonebook_delete
};

static CrudAPI text_api = {
    .create_arr_str = at_text_create,
    .read = at_text_read,
    .update = NULL,
    .del = at_text_delete
};

void record_noise(){
    // send command to start or stop recording noise
    

    at_send_command(start_command, NULL); 
    sem_wait(&noise_sem);
    recording_noise = 1;
    sem_post(&noise_sem);

    while(1){
        sem_wait(&noise_sem);
        if (recorded_noise_samples > 30*8000){
            recording_noise = 0;

            sem_post(&noise_sem);
            usleep(50000);
            break;
        }
        sem_post(&noise_sem);
    }
    at_send_command(stop_command, NULL);
    

}

int at_send_command(const char* command, char** output){
    int sockfd;
    char recv_buf[TCP_BUFFER];
    int b_recv;

    #if 1

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ERROR("socket failed: %s", strerror(errno));
        return 0;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ERROR("connect failed: %s", strerror(errno));
        close(sockfd);
        return 0;
    }

    if (send(sockfd, command, strlen(command), 0) < 0) {
        ERROR("send failed: %s", strerror(errno));
        close(sockfd);
        return 0;
    }

    b_recv = recv(sockfd, recv_buf, TCP_BUFFER - 1, 0);
    if (b_recv < 0) {
        ERROR("recv failed: %s", strerror(errno));
        close(sockfd);
        return 0;
    }

    close(sockfd);

    if (output != NULL){
        *output = malloc(b_recv+1);


        memcpy(*output, recv_buf, b_recv);
        (*output)[b_recv] = '\0';
    }
    
    return b_recv;
    #endif

    // return crash_str_size;
    
}

void at_init(char* server, int port){
    server_addr.sin_family = AF_INET;
    if (server != NULL){
        server_addr.sin_addr.s_addr = inet_addr(server);
    } else {
        server_addr.sin_addr.s_addr = inet_addr(ucontroller_address);
    }
    if (port){
        server_addr.sin_port = htons(port);
    } else {
        server_addr.sin_port = htons(ucontroller_cmd_port);
    }
}

char* at_help(){
    char* buffer = malloc(700);
    FILE* helptext = fopen("at_help.txt", "r");
    fread(buffer, 700, sizeof(char), helptext);
    fclose(helptext);
    return buffer;
}

int at_process_command(const char* command, char** output){
	char buf[TCP_BUFFER];
	char *s;
    const char *param1 = NULL, *param2 = NULL, *param3 = NULL, *param4 = NULL;
	int i = 0;
	enum { CMD_NONE = 0, CMD_PHONEBOOK, CMD_TEXT, CMD_CALL_MAKE, CMD_CALL_HANG, NETWORK, PHONEBOOK_MODE, OWN_NUMBER, RECORD_NOISE, HELP } cmd = CMD_NONE;

	DEBUG("AT: process command: '%s'", command);

	strcpy(buf, command); // TODO fammi avere command senza "!test" (ts cmd keyword)

	s = strtok(buf, " ");
	
    while(s != NULL) {
		DEBUG("checking s %s", s);
		if(i == 0) {
			if(!strcmp(s, "phonebook")) {
				cmd = CMD_PHONEBOOK;
			} else if(!strcmp(s, "text")) {
				cmd = CMD_TEXT;
			} else if(!strcmp(s, "call")) {
				cmd = CMD_CALL_MAKE;
			} else if(!strcmp(s, "hang")) {
				cmd = CMD_CALL_HANG;
            } else if (!strcmp(s, "network")) {
                cmd = NETWORK;
            } else if (!strcmp(s, "pb_mode")) {
                cmd = PHONEBOOK_MODE;
            } else if (!strcmp(s, "mynumber")) {
                cmd = OWN_NUMBER;
            } else if (!strcmp(s, "noise")){
                cmd = RECORD_NOISE;
            } else if (!strcmp(s, "help")){
                cmd = HELP;
            }
		} else if(i == 1) {
			param1 = s;
		} else if (i == 2){
			param2 = s;
		} else if (i == 3){
			param3 = s;
		} else {
			param4 = s;
		}
        if (cmd == CMD_TEXT && (param1 != NULL && strcmp(param1, "send")==0) && param2 != NULL){
            param3 = command + strlen("text") + 1 + strlen(param1) + 2 + strlen(param2); 
            break;
        }
        s = strtok(NULL, " ");
		i++;
	}

    char* cmd_str = NULL; char** cmd_list = NULL;
	switch(cmd) {
		case CMD_NONE:
			return 1;  /* Command not handled by at */
		case CMD_PHONEBOOK:  /* /test join <channelID> [optionalCannelPassword] */
			if(param1) {
                DEBUG("checking params 1..4: %s, %s, %s, %s", param1, param2, param3, param4);
                DEBUG("checking if it is a read: %i", strcmp(param1, "read"));

                if (param2 && param3 && param4){ //update a contact in position "param2"
                    if (strcmp(param1, "create") == 0 && allow_create_contacts){
                        DEBUG("match for edit contact!");
                        at_send_command(cmd_str = phonebook_api.create_str(param2, param3, param4), output);
                        break;
                    }
                }
				else if (param2 && param3){
                    DEBUG("got param2 & param3!");
                    if (strcmp(param1, "create") == 0 && allow_create_contacts){
                        DEBUG("match for create contact!");
                        at_send_command(cmd_str = phonebook_api.create_str(NULL, param2, param3), output);
                        break;
                    } else if (strcmp(param1, "update") == 0 && allow_create_contacts){
                        DEBUG("match for update contact!");
                        at_send_command(cmd_str = phonebook_api.update(param2, param3), output);
                        break;
                    
                    } else if (strcmp(param1, "read") == 0){
                        DEBUG("match for read contact!");
                        at_send_command(cmd_str = phonebook_api.read(param2, param3), output);
                        break;
                    }
                } else if (param2){
                    if (strcmp(param1, "delete") == 0 && allow_delete_contacts){
                        DEBUG("match for delete contact!");
                        at_send_command(cmd_str = phonebook_api.del(param2), output);
                        break;
                    }
                }
			} 
            return -1;
        case CMD_TEXT:
            if (param1){
                if (param2 && param3){
                    if (strcmp(param1, "delete") == 0 && allow_delete_sms){
                        at_send_command(cmd_str = text_api.del(param2, param3), output);
                        break;
                    } else if (strcmp(param1, "send") == 0 && allow_send_sms){
                        
                        char** _cmd_list = at_set_text_mode(1);
                        cmd_list = _cmd_list;
                        while (*_cmd_list != NULL){
                            at_send_command(*_cmd_list, NULL); 
                            _cmd_list++;
                        }
                        
                        while (cmd_list != NULL && *cmd_list != NULL) {
                            free(*cmd_list);
                            cmd_list++;
                        }

                        _cmd_list = text_api.create_arr_str(param2, param3);
                        cmd_list = _cmd_list;
                        while (*_cmd_list != NULL){
                            at_send_command(*_cmd_list, NULL); 
                            _cmd_list++;
                        }

                        break;
                    } else if (strcmp(param1, "read") == 0){

                        
                        char** _cmd_list = at_set_text_mode(0);
                        cmd_list = _cmd_list;
                        while (*_cmd_list != NULL){
                            at_send_command(*_cmd_list, NULL); 
                            _cmd_list++;
                        }
                        // free(cmd_str);
                        at_send_command(cmd_str = text_api.read(NULL, param3), output);
                        break;

                    }
                }
                else if (param2){
                    if (strcmp(param1, "delete") == 0 && allow_delete_sms){
                        at_send_command(cmd_str = text_api.del(param2, NULL), output);
                        break;
                    }
                    if (strcmp(param1, "read") == 0){

                        char** _cmd_list = at_set_text_mode(0);
                        cmd_list = _cmd_list;
                        while (*_cmd_list != NULL){
                            at_send_command(*_cmd_list, NULL); 
                            _cmd_list++;
                        }
                        // free(cmd_str);
                        at_send_command(cmd_str = text_api.read(param2, NULL), output);
                        break;
                    }
                }
            }
            return -1;
        case NETWORK: {
            
                char** _output = malloc(3*sizeof(char*));
                char** output_start = _output;
                char** _cmd_list = at_check_network_status();
                cmd_list = _cmd_list;
                while (*_cmd_list != NULL){
                    at_send_command(*_cmd_list, _output);
                    _cmd_list++;
                    _output++;
                }
                _output = output_start;
                int total_copied = 0;
                *output = malloc(250);
                for (i=0; i < 3; i++){
                    memcpy(*output + total_copied, *_output, strlen(*_output));
                    int copied = strlen(*_output);
                    (*output)[total_copied + copied] = '\n';
                    total_copied += (copied+1);
                    _output++;
                }
                (*output)[total_copied] = '\0';
            }
            break;
        case CMD_CALL_MAKE:
            
            
            if (param1 && allow_make_call){
                at_send_command(at_call_make(param1), NULL);
                break;
            }
            return -1;
        case CMD_CALL_HANG:
            
            
            at_send_command(at_call_hang(), NULL);
            break;
        case PHONEBOOK_MODE:
            
            
            if (param1){
                int flag = 1; //by defaults it's true, i.e. answers only to numbers in the phonebook
                flag = !strcmp(param1, "false") ? 0 : flag;
                at_send_command(at_answer_phonebook_only(flag), NULL);
                break;
            }
            return -1;
        case OWN_NUMBER:
            
            
            at_send_command(at_get_own_number(), output);
            break;
        case RECORD_NOISE:
            record_noise();
            
            break;
        case HELP:
            
            
            *output = at_help();
            break;
	}

    if (cmd_str != NULL)
        free(cmd_str); 
    
    char** original_cmd_list = cmd_list;  // Save original pointer
    while (cmd_list != NULL && *cmd_list != NULL) {
        free(*cmd_list);
        cmd_list++;
    }

    if(original_cmd_list != NULL){
        free(original_cmd_list);   // Free the whole array of char*    
    }
	return 0;  /* AT handled command */
}