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
    if (!result) return NULL;
    snprintf(result, bufsize, "AT+CPBW=%s", index);  // delete by number
    return result;
}

char* at_text_create(char* dest, char* text){
     return NULL;
}

char* at_text_read(char* dest, char* text){
     return NULL;
} 

//TODO
char* at_text_delete(char* dest, char* text){
    return NULL;
} 

//TODO
char* at_call_make(char* name){
    char* buffer = malloc(20);  // allocate memory on the heap
    if (buffer != NULL) {
        snprintf(buffer, 20, "ATD%s;\n", name);
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

char* at_set_text_mode(char* mode){
    return NULL;
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

    printf("[DEBUG] b_recv/strlen(buf): %i/%i\n", b_recv, strlen(recv_buf));
    
    *output = malloc(b_recv);


    memcpy(*output, recv_buf, b_recv-1);
    output[b_recv] = "\0";

    printf("[DEBUG] at command recv_buf: %s\n", recv_buf);
    printf("[DEBUG] copied recv_buf into output: %s\n", *output);
    
    return b_recv;
}

void at_init(char* server, int port){
    server_addr.sin_family = AF_INET;
    if (server != NULL){
        server_addr.sin_addr.s_addr = inet_addr(server);
    } else {
        server_addr.sin_addr.s_addr = inet_addr(TCP_SERVER);
    }
    if (port != NULL){
        server_addr.sin_port = htons(port);
    } else {
        server_addr.sin_port = htons(TCP_LISTEN_PORT);
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
			} else if(!strcmp(s, "call_make")) {
				cmd = CMD_CALL_MAKE;
			} else if(!strcmp(s, "call_hang")) {
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

    char* cmd_str;
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
            // free(cmd_str); //TODO
            return -1;
	}

    // free(cmd_str); //TODO
	return 0;  /* AT handled command */
}