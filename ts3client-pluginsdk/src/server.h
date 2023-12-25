#ifndef SERVER_H
#define SERVER_H
#include <arpa/inet.h>
// #include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <coap3/coap.h>
#include <netdb.h>
#include <coap3/coap_debug.h>

#include <semaphore.h>
#include <kissfft/kiss_fft.h>
#include <kissfft/kiss_fftr.h>

// char* server_address;
// char* server_port;

int resolve_address(const char *host, const char *service, coap_address_t *dst);
// void* resps_hndl(coap_context_t *context, coap_session_t *session, coap_pdu_t *sent, coap_pdu_t *received, const coap_mid_t id);
int connect_to_coap_server();
int send_voice(short* samples, int sample_counter, int channels);
int observe_voice(void* callback, unsigned char* data);

//in voice_buffer we write the voice data we receive from the coap server on the arduino
// extern sem_t sem_voice_buffer; 


#endif