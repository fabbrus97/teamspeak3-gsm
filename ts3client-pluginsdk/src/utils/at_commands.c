#include "at_commands.h"

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

void utf8_to_ucs2_encoder(char* src, char* output){
    //change encoding
    size_t bufsize = strlen(src);
    iconv_t cd = iconv_open("UCS-2BE", "UTF-8");
    size_t out_len = bufsize * 2; // Worst case
    char* ucs2_str = malloc(out_len); memset(ucs2_str, 0, out_len);
    
    iconv(cd, &src, &bufsize, &ucs2_str, &out_len);
    iconv_close(cd);

    // converting to hex string
    size_t ucs2_len = (&out_len) - (size_t)ucs2_str; // pointer arithmetic
    char* hex_str = malloc(ucs2_len * 2 + 1);
        for (size_t i = 0; i < ucs2_len; ++i) {
        sprintf(hex_str + i * 2, "%02X", (unsigned char)ucs2_str[i]);
    }
    hex_str[ucs2_len * 2] = '\0';
    free(ucs2_str);
    output = hex_str;
}

/*
    // AT+CMGS=<da>[,<toda>],<CR> send sms
    da: destination addess in string format (string should be included in quotation marks)
    toda: type of address (145 or 129)
*/
char* at_text_create(char* dest, char* text){
     
    size_t bufsize = 1024; 
    char* result = malloc(bufsize);
    
    /* 
    * converting to usc2 encoding to support emojis 😍😍😍 
    */
    char* hex_dest, hex_text; 
    utf8_to_ucs2_encoder(dest, hex_dest);    
    utf8_to_ucs2_encoder(text, hex_text);    

    snprintf(result, bufsize, "AT+CMGS=\"%s\"\r%s%c", hex_dest, hex_text, 0x1A);

    free(hex_dest);
    free(hex_text);
    
    return result;
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
char* at_text_read(int index, char* mode){
     
    size_t bufsize = 50;
    char* result = (char*)malloc(bufsize);
    if (index != NULL)
        snprintf(result, bufsize, "AT+CMGR=%i", index);
    else {
        if(strcmp(mode, "sent") == 0){
            snprintf(result, bufsize, "AT+CMGL=%s", "STO SENT");
        } else if(strcmp(mode, "unread") == 0){
            snprintf(result, bufsize, "AT+CMGL=%s", "REC UNREAD");
        } else if(strcmp(mode, "read") == 0){
            snprintf(result, bufsize, "AT+CMGL=%s", "REC READ");
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
char* at_text_delete(int index, char* flag){
    
    int delflag = 0;
    if (flag != NULL && strcmp(flag, "read") == 0){
        delflag = 1;
    }
    if (flag != NULL && strcmp(flag, "all") == 0){
        delflag = 4;
    }

    size_t maxlen = 25;
    char* result = malloc(maxlen);
    snprintf(result, maxlen, "AT+CMGD=%i,%i", index, delflag);
            
    return result;
} 

char* at_call_make(char* name){
    char* buffer = malloc(40);  // allocate memory on the heap
    if (buffer != NULL) {
        snprintf(buffer, 40, "ATD%s;\n", name);
    }
    return buffer;
    
}

char* at_call_hang(){
    char* buffer = malloc(5);
    strcpy(buffer, "ATH\n");
    return buffer;
}

char* at_call_accept(){
    char* buffer = malloc(5);
    strcpy(buffer, "ATA\n");
    return buffer;
}

char* at_call_wait(){
    return NULL;
}

char* at_call_merge(){
    return NULL;
}

char* at_can_call(int flag){
    return NULL;
}

char* at_can_text(int flag){
    return NULL;
}

char* at_answer_phonebook_only(int flag){
    return NULL;
}

char** at_set_text_mode(){
    char* textmode = "AT+CMGF=1";
    char* encoding = "AT+CSCS=\"UCS2\"";
    char* command1 = malloc(strlen(textmode)+1);
    char* command2 = malloc(strlen(encoding)+1);
    char** commands = malloc(sizeof(command1)*2+1);
    commands[0] = command1; commands[1] = command2; commands[2] = NULL;
    return commands;
} 

//TODO
char* at_check_network_status(){
//     AT+CSQ        --> Check signal quality
// AT+CREG?      --> Check network registration status
// AT+COPS?      --> Check current operator
    return NULL;
}

char* at_send_AT(){
    char* buffer = malloc(4);  // allocate memory on the heap
    strcpy(buffer, "AT\n");
    return buffer;
}

static CrudAPI phonebook_api = {
    .create = at_phonebook_create,
    .read = at_phonebook_read,
    .update = NULL,
    .del = at_phonebook_delete
};

static CrudAPI text_api = {
    .create = at_text_create,
    .read = at_text_read,
    .update = NULL,
    .del = at_text_delete
};

int at_send_command(char* command, char** output){
    int sockfd;
    char recv_buf[TCP_BUFFER];
    int b_recv;

    // int crash_str_size = 33;
    // *output = malloc(crash_str_size+1);
    // *output = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\0"; //33
    
    #if 1

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket failed");
        return 0;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        close(sockfd);
        return 0;
    }

    if (send(sockfd, command, strlen(command), 0) < 0) {
        perror("send failed");
        close(sockfd);
        return 0;
    }

    b_recv = recv(sockfd, recv_buf, TCP_BUFFER - 1, 0);
    if (b_recv < 0) {
        perror("recv failed");
        close(sockfd);
        return 0;
    }

    close(sockfd);

    // recv_buf[b_recv] = '\0';
    // printf("[DEBUG] b_recv/strlen(buf): %i/%i\n", b_recv, strlen(recv_buf));
    
    if (output != NULL){
        *output = malloc(b_recv+1);


        memcpy(*output, recv_buf, b_recv);
        (*output)[b_recv] = '\0';
    }
    // printf("[DEBUG] at command recv_buf: %s\n", recv_buf);
    // printf("[DEBUG] copied recv_buf into output: %s\n", *output);
    
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
    if (port != NULL){
        server_addr.sin_port = htons(port);
    } else {
        server_addr.sin_port = htons(ucontroller_cmd_port);
    }
}

int at_process_command(const char* command, char** output){
	char buf[TCP_BUFFER];
	char *s, *param1 = NULL, *param2 = NULL, *param3 = NULL, *param4 = NULL;
	int i = 0;
    //TODO completa comandi
	enum { CMD_NONE = 0, CMD_PHONEBOOK, CMD_TEXT, CMD_CALL_MAKE, CMD_CALL_HANG } cmd = CMD_NONE;

	printf("AT: process command: '%s'\n", command);

	strcpy(buf, command); // TODO fammi avere command senza "!test" (ts cmd keyword)

	s = strtok(buf, " ");
	
    while(s != NULL) {
		printf("[DEBUG] checking s %s\n", s);
		if(i == 0) {
			if(!strcmp(s, "phonebook")) {
				cmd = CMD_PHONEBOOK;
			} else if(!strcmp(s, "text")) {
				cmd = CMD_TEXT;
			} else if(!strcmp(s, "call")) {
				cmd = CMD_CALL_MAKE;
			} else if(!strcmp(s, "hang")) {
				cmd = CMD_CALL_HANG;
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
		s = strtok(NULL, " ");
		i++;
	}

    char* cmd_str; char** cmd_list;
	switch(cmd) {
		case CMD_NONE:
			return 1;  /* Command not handled by at */
		case CMD_PHONEBOOK:  /* /test join <channelID> [optionalCannelPassword] */
			if(param1) {
                printf("[DEBUG] checking params 1..4: %s, %s, %s, %s\n", param1, param2, param3, param4);
                printf("[DEBUG] checking if it is a read: %i\n", strcmp(param1, "read"));

                if (param2 && param3 && param4){ //update a contact in position "param2"
                    if (strcmp(param1, "create") == 0){
                        printf("[DEBUG] match for edit contact!\n");
                        at_send_command(cmd_str = phonebook_api.create(param2, param3, param4), output);
                        break;
                    }
                }
				else if (param2 && param3){
                    printf("[DEBUG] got param2 & param3!\n");
                    if (strcmp(param1, "create") == 0){
                        printf("[DEBUG] match for create contact!\n");
                        at_send_command(cmd_str = phonebook_api.create(NULL, param2, param3), output);
                        break;
                    } else if (strcmp(param1, "update") == 0){
                        printf("[DEBUG] match for update contact!\n");
                        at_send_command(cmd_str = phonebook_api.update(param2, param3), output);
                        break;
                    
                    } else if (strcmp(param1, "read") == 0){
                        printf("[DEBUG] match for read contact!\n");
                        at_send_command(cmd_str = phonebook_api.read(param2, param3), output);
                        break;
                    }
                } else if (param2){
                    if (strcmp(param1, "delete") == 0){
                        printf("[DEBUG] match for delete contact!\n");
                        at_send_command(cmd_str = phonebook_api.del(param2), output);
                        break;
                    }
                }
			} 
            return -1;
        case CMD_TEXT:
            if (param1){
                if (param2 && param3){
                    if (strcmp(param1, "delete") == 0){
                        at_send_command(cmd_str = text_api.del(param2, param3), output);
                        break;
                    } else if (strcmp(param1, "send") == 0){
                        
                        char** _cmd_list = at_set_text_mode();
                        cmd_list = _cmd_list;
                        while (_cmd_list != NULL){
                            at_send_command(*_cmd_list, output);
                            _cmd_list++;
                        }
                        at_send_command(cmd_str = text_api.create(param2, param3), output);
                        break;
                    }
                }
                else if (param2){
                    if (strcmp(param1, "delete") == 0){
                        at_send_command(cmd_str = text_api.del(param2, NULL), output);
                        break;
                    }
                    if (strcmp(param1, "read") == 0){
                        at_send_command(cmd_str = text_api.read(param2, NULL), output);
                        break;
                    }
                } else if (param3){
                    if (strcmp(param1, "read") == 0){
                        at_send_command(cmd_str = text_api.read(NULL, param3), output);
                        break;
                    }
                }
            }
            

            return -1;
            
	}

    free(cmd_str); 
    free(cmd_list); 
	return 0;  /* AT handled command */
}