#include "server.h"
#include "ts3_functions.h"


// char* server_address = "127.0.0.1";
char* server_address = "192.168.1.18";
char* server_port = "5683";
static unsigned int token_obs = 0;
sem_t sem_voice_buffer; 
static int have_response = 0;
static coap_context_t *ctx;
static coap_session_t *session;
static short* voice_buffer;
static int sample_counter = 0;
int socket_desc;
struct sockaddr_in server_addr, client_addr;
char /* server_message[2000], */ client_message[2000];
int client_struct_length; // = sizeof(client_addr);


static coap_pdu_t* coap_new_request(coap_optlist_t **options, unsigned char *data, size_t length);


//from https://github.com/obgm/libcoap-minimal
int resolve_address(const char *host, const char *service, coap_address_t *dst) {

  struct addrinfo *res, *ainfo;
  struct addrinfo hints;
  int error, len=-1;


  memset(&hints, 0, sizeof(hints));
  memset(dst, 0, sizeof(*dst));
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_family = AF_UNSPEC;

  error = getaddrinfo(host, service, &hints, &res);

  if (error != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
    return error;
  }

  for (ainfo = res; ainfo != NULL; ainfo = ainfo->ai_next) {
    switch (ainfo->ai_family) {
    case AF_INET6:
    case AF_INET:
      len = dst->size = ainfo->ai_addrlen;
      memcpy(&dst->addr.sin6, ainfo->ai_addr, dst->size);
      goto finish;
    default:
      ;
    }
  }

 finish:
  freeaddrinfo(res);
  return len;
}

int resps_hndl(coap_session_t *session, coap_pdu_t *sent, coap_pdu_t *received, const coap_mid_t id){
      have_response = 1;
      printf("we got a response\n");
      coap_show_pdu(LOG_WARNING, received);
      return COAP_RESPONSE_OK;
};

int voice_hndl(coap_session_t *session, coap_pdu_t *sent, coap_pdu_t *received, const coap_mid_t id){ 
      have_response = 1;
      printf("we got voice data\n");
      // coap_show_pdu(LOG_WARNING, received);
      const uint8_t * payload; 
      size_t len_payload;
      coap_get_data(received, &len_payload, &payload) ;

      sem_wait(&sem_voice_buffer);
      if (sample_counter == 0){
        //write to buffer
        voice_buffer = (short*)payload;
        sample_counter = 512; //fixed value - the server buffer is 512 sample 
      }
      sem_post(&sem_voice_buffer);
      
      return COAP_RESPONSE_OK;
};

int start_udp_socket(){
    
    // Clean buffers:
    // memset(server_message, '\0', sizeof(server_message));
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
    server_addr.sin_port = htons(UDP_LISTEN_PORT);
    server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    
    // Bind to the set port and IP:
    if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        printf("Couldn't bind to the port\n");
        return -1;
    }
    printf("Done with binding\n");
    
    printf("Listening for incoming messages...\n\n");
    return 0;
}

//this function sends commands (e.g. "call +39123456789")
int connect_to_coap_server(){

    ctx = NULL;
    session = NULL;
    coap_address_t dst;
    
    coap_startup();

    /* Set logging level */
    coap_set_log_level(LOG_WARNING);

    /* resolve destination address where server should be sent */
    if (resolve_address(server_address, server_port, &dst) < 0) {
      coap_log_impl(LOG_ERR, "failed to resolve address\n");
      goto finish;
    }

    /* create CoAP context and a client session */
    if (!(ctx = coap_new_context(NULL))) {
      coap_log_impl(LOG_EMERG, "cannot create libcoap context\n");
      goto finish;
    }
    /* Support large responses */
    coap_context_set_block_mode(ctx,
                    COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

    if (!(session = coap_new_client_session(ctx, NULL, &dst,
                                                    COAP_PROTO_UDP))) {
      coap_log_impl(LOG_EMERG, "cannot create client session\n");
      goto finish;
    }

    return EXIT_SUCCESS;
    
    finish:

    coap_session_release(session);
    coap_free_context(ctx);
    coap_cleanup();

    return EXIT_FAILURE;

}

// void convert_short_to_uint8(short * samples, int count, uint8_t* out){
//   uint8_t uint8Value = 0;
//   for (int i = 0; i < count; i++){
//     if (samples[i] < 0) uint8Value = (uint8_t)(((127.0)*(1 - (samples[i] / -32767.0))));
//     else uint8Value = (uint8_t)((127*(samples[i]/32767.0)) + 128); ///65536.0)));
//     out[i] = uint8Value;
    
//   }
// }
void convert_short_to_uint8(short * samples, int count, uint8_t* out){
  uint8_t uint8Value = 0;
  int pos=0;
  for (int i = 0; i < count; i++){
  //   if (samples[i] < 0) uint8Value = (uint8_t)(((127.0)*(1 - (samples[i] / -32767.0))));
  //   else uint8Value = (uint8_t)((127*(samples[i]/32767.0)) + 128); ///65536.0)));
  //    out[i] = uint8Value;
  //    printf("read %i -> %i\n", samples[i], uint8Value);
  //  }
    uint8_t byte1 = samples[i] & 0xFF;
    uint8_t byte2 = ((samples[i] >> 8) & 0xFF);
    out[pos] = byte1; out[pos+1] = byte2;
    pos+=2;
 }
}


void downsample_soxr(short* ibuffer, short* obuffer, int ilen, int olen) {
    // Set input and output rates
    soxr_io_spec_t ioSpec = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);

    // Create a SoXR resampler
    soxr_t resampler = soxr_create(ISIZE, OSIZE, 1, NULL, &ioSpec, NULL, NULL);

    // Determine input and output buffer sizes
    // int obuf_size = (size_t)(ilen * OSIZE / ISIZE + .5);
    // printf("soxr: input size is %i, output size is %i\n", ilen, obuf_size);
    printf("soxr: allocating memory\n");
    // obuffer = (short*)malloc(sizeof(short) * obuf_size);

    size_t idone, odone;
    soxr_error_t error;

    printf("soxr: resampling...\n");
    error = soxr_process(resampler, ibuffer, ilen, &idone, obuffer, olen, &odone);
    printf("soxr: done\n");

    if (error) {
      fprintf(stderr, "Error during resampling: %s\n", soxr_strerror(error));
      obuffer = NULL;
    }

    // Clean up
    soxr_delete(resampler);
}

// Function to apply gain reduction to audio samples
void apply_gain(short* samples, size_t num_samples, double gain) {
    for (size_t i = 0; i < num_samples; ++i) {
        // Apply gain to each sample
        samples[i] = (short)(samples[i] * gain);
    }
}

int send_voice(short* samples, int sample_counter, int channels){

    // sample_counter = sample_counter > 100 ? 100 : sample_counter; //TODO
    int obuf_size = (size_t)(sample_counter * OSIZE / ISIZE + .5);    
    short downsampled[obuf_size];
    uint8_t tmp[116];
    //0 means the semaphore is shared between threads
    //1 is the initial value of the semaphore
    sem_init(&sem_voice_buffer, 0, 1);

    // demo_write_voice(samples, sample_counter);

    

    int result = EXIT_FAILURE;

    /* coap_register_response_handler(ctx, response_handler); */
    coap_register_response_handler(ctx, (coap_response_handler_t)resps_hndl);
    

    printf("send_voice: trying to add the payload\n");

    // int out_samples = (size_t)(sample_counter * OSIZE / ISIZE + .5);
    // short* downsampled = (short*)malloc(sizeof(short) * out_samples);

   
    printf("downsampling using soxr\n");
    // downsample_soxr(samples, downsampled, sample_counter, out_samples);    
    
    size_t idone = 0, odone = 0;    
    soxr_error_t error;    
    
    // Read from input file    
    
    // short obuf[obuf_size];
    printf("output buffer has size %i\n", obuf_size);    

    soxr_io_spec_t ioSpec = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);   
    soxr_t resampler = soxr_create(ISIZE, OSIZE, 1, NULL, &ioSpec, NULL, NULL);     
    // Resample the input buffer    
    error = soxr_process(resampler, samples, sample_counter, &idone, downsampled, obuf_size, &odone);    
    // error = soxr_oneshot(ISIZE, OSIZE, 1, /* Rates and # of channels. */
    //   samples, sample_counter, NULL,                              /* Input. */
    //   downsampled, obuf_size, &odone,                             /* Output. */
    //   NULL, NULL, NULL);                             /* Default configuration.*/

    if (error) {    
        fprintf(stderr, "Error during resampling: %s\n", soxr_strerror(error));    
    }    
    
    // apply_gain(downsampled, odone, 0.2);

    // FILE* fin_orig = fopen("test_voice_no_ds.pcm", "a");
    // FILE* fin_ds = fopen("test_voice_ds.pcm", "a");

    // fwrite(samples, sizeof(short), sample_counter, fin_orig);
    // fwrite(downsampled, sizeof(short), obuf_size, fin_ds);

    // fclose(fin_orig);
    // fclose(fin_ds);

    printf("converting to uint8_t\n");
    // uint8_t* data = (uint8_t *)malloc(sizeof(uint8_t) * odone);
    // uint8_t* data = (uint8_t *)malloc(sizeof(uint8_t) * sample_counter);
    
    // convert_short_to_uint8(samples, sample_counter, data);
    uint8_t data[odone*2];

    convert_short_to_uint8(downsampled, odone, data);
    // fwrite(data, sizeof(uint8_t), odone, fin_ds);
    // fclose(fin_ds);

    printf("done, sending...\n");
    printf("total size to send: %i expected send: %i\n", odone*2, (odone*2)/116);
    
    int sent = 0;
    int mycounter = 1;
    while (sent < odone*2){
      int available = odone*2 - sent > 116 ? 116 : odone*2 - sent;
      memcpy(tmp, data, available);

      printf("send %i first 10 bytesa are: ", mycounter);
      for (int i = 0; i<10; i++) printf("%i ", tmp[i]);
      printf("\n");
      mycounter += 1;

      //samples: signed 16 bit
      coap_pdu_t *pdu = NULL;
      /* construct CoAP message */
      pdu = coap_pdu_init(COAP_MESSAGE_NON,
                          COAP_REQUEST_CODE_POST,
                          coap_new_message_id(session),
                          coap_session_max_pdu_size(session));
      if (!pdu) {
        coap_log_impl(LOG_EMERG, "cannot create PDU\n" );
        // goto finish;
        return result;
      }
      
      printf("adding payload...\n");

      if (!coap_add_data(pdu, available, &(data[sent]))){
          coap_log_impl(LOG_ERR, "cannot add payload\n");
          // goto finish;
          return result;
      }

      printf("send_voice: payload added\n");

      /* add a Uri-Path option */
      const char *tag = "talk";
      // const char *tag = "voice_data_rcv";
      coap_add_option(pdu, COAP_OPTION_URI_PATH, 4,
                      tag);

      coap_show_pdu(LOG_WARNING, pdu);
      /* and send the PDU */
      if (coap_send(session, pdu) == COAP_INVALID_MID) {
        coap_log_impl(LOG_ERR, "cannot send CoAP pdu\n");
        // goto finish;
        return result;
      }

      printf("going in while...\n");

      // coap_io_process(ctx, COAP_IO_NO_WAIT);

      int retries=30;

      while (have_response == 0 && retries > 0){
        coap_io_process(ctx, COAP_IO_NO_WAIT);
        retries--;
      }

      // struct timespec myts; 
      // myts.tv_sec=0;
      // myts.tv_nsec = 100;
      // nanosleep(&myts, &myts);

      have_response = 0;

      result = EXIT_SUCCESS;

      printf("trying to reset tmp...\n");
      memset(tmp, 0, 116);
      printf("done\n");

      sent += available;

    }

    printf("I'm out of for, coap data sent\n");
    
    // finish:

    // printf("coap_session_release\n");
    // coap_session_release(session);
    // printf("coap_free_context\n");
    // coap_free_context(ctx);
    // printf("coap_cleanup\n");
    // coap_cleanup();

    soxr_delete(resampler);
    
    // printf("freeing coap data\n");
    // free(data);
    // printf("freeing coap data done\n");


    return result;
}

int observe_voice(void* callback, unsigned char* data){
    coap_pdu_t *pdu = NULL;

    int result = EXIT_FAILURE;; 

    /* coap_register_response_handler(ctx, response_handler); */
    coap_register_response_handler(ctx, (coap_response_handler_t)voice_hndl);
    /* construct CoAP message */
    pdu = coap_new_request(NULL, data, sizeof(data)); //TODO get options???

    printf("send_voice: payload added\n");

    /* add a Uri-Path option */
    coap_add_option(pdu, COAP_OPTION_URI_PATH, 10,
                    "voice_data_get");

    coap_show_pdu(LOG_WARNING, pdu);
    /* and send the PDU */
    if (coap_send(session, pdu) == COAP_INVALID_MID) {
      coap_log_impl(LOG_ERR, "cannot send CoAP pdu\n");
      goto finish;
    }

    printf("going in while...\n");

    while (have_response == 0)
      coap_io_process(ctx, COAP_IO_WAIT);

    have_response = 0;

    result = EXIT_SUCCESS;

    finish:

    // printf("coap_session_release\n");
    // coap_session_release(session);
    // printf("coap_free_context\n");
    // coap_free_context(ctx);
    // printf("coap_cleanup\n");
    // coap_cleanup();

    return result;
}

/* int main(){

    //TODO demo

    connect_to_coap_server();
    return 0;

} */

void start_udp_server(){
    // Receive client's message:
    for(;;){
        if (recvfrom(socket_desc, client_message, sizeof(client_message), 0,
            (struct sockaddr*)&client_addr, &client_struct_length) < 0){
            printf("Couldn't receive\n");
            return -1;
        }
        printf("Received message from IP: %s and port: %i\n",
              inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        short buffer[1];
        // ts3func.processCustomCaptureData("ts3callbotplayback", buffer, 0);
        
        struct timespec myts; 
        myts.tv_sec=0;
        myts.tv_nsec = 10000; //10 millisec
        nanosleep(&myts, &myts);
    }
    // Respond to client:
    // strcpy(server_message, client_message);
    
    // if (sendto(socket_desc, server_message, strlen(server_message), 0,
    //      (struct sockaddr*)&client_addr, client_struct_length) < 0){
    //     printf("Can't send\n");
    //     return -1;
    // }
    
    // Close the socket:
    // close(socket_desc);
    
}

static coap_pdu_t* coap_new_request(coap_optlist_t **options, unsigned char *data, size_t length) {

    coap_pdu_t *pdu;
    
    /* Create the pdu with the appropriate options */
    pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_GET, coap_new_message_id(session),
                        coap_session_max_pdu_size(session));
    if (!pdu)
      return NULL;

    /*
    * Create uniqueness token for this request for handling unsolicited /
    * delayed responses
    */
    token_obs++;
    if (!coap_add_token(pdu, sizeof(token_obs), (unsigned char*)&token_obs)) {
      coap_log(LOG_DEBUG, "cannot add token to request\n");
      goto error;
    }

    /* we want to observe this resource */
    if (!coap_insert_optlist(options,
                            coap_new_optlist(COAP_OPTION_OBSERVE,
                                        COAP_OBSERVE_ESTABLISH, NULL)))
      goto error;
  

    /* ... Other code / options etc. ... */

    /* Add in all the options (after internal sorting) to the pdu */
    if (!coap_add_optlist_pdu(pdu, options))
      goto error;

    if (data && length) {
      /* Add in the specified data */
      if (!coap_add_data(pdu, length, data))
        goto error;
    }

    return pdu;

  error:

    coap_delete_pdu(pdu);
    return NULL;

}

/*
  TODO:
  1. registra dispositivo audio custom
  2. fornisci audio al dispositivo custom
    processCustomCaptureData
  3. (?) ottieni audio dal dispositivo custom 
    acquireCustomPlaybackData
    (non so se mi serve, o basta ts3plugin_onEditPlaybackVoiceDataEvent)
*/ 