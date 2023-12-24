#include "server.h"

char* server_address = "192.168.1.17";
char* server_port = "5683";
static unsigned int token_obs = 0;
sem_t sem_voice_buffer; 
static int have_response = 0;
static coap_context_t *ctx;
static coap_session_t *session;
static short* voice_buffer;
static int sample_counter = 0;

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

uint8_t* convert_short_to_uint8(short * samples, int count){
  uint8_t *data = malloc(count * sizeof(uint8_t));

  for (int i = 0; i < count; i++){
    uint8_t uint8Value = (uint8_t)((255.0*((samples[i]+32767)/65536.0)));
    data[i] = uint8Value;
  }

  return data;
}

int send_voice(short* samples, int sample_counter, int channels){

    sample_counter = sample_counter > 100 ? 100 : sample_counter; 
    //0 means the semaphore is shared between threads
    //1 is the initial value of the semaphore
    sem_init(&sem_voice_buffer, 0, 1);

    // demo_write_voice(samples, sample_counter);

    //samples: signed 16 bit
    coap_pdu_t *pdu = NULL;

    int result = EXIT_FAILURE;; 

    /* coap_register_response_handler(ctx, response_handler); */
    coap_register_response_handler(ctx, (coap_response_handler_t)resps_hndl);
    /* construct CoAP message */
    pdu = coap_pdu_init(COAP_MESSAGE_NON,
                        COAP_REQUEST_CODE_POST,
                        coap_new_message_id(session),
                        coap_session_max_pdu_size(session));
    if (!pdu) {
      coap_log_impl(LOG_EMERG, "cannot create PDU\n" );
      goto finish;
    }

    printf("send_voice: trying to add the payload\n");

    uint8_t* data = convert_short_to_uint8(samples, sample_counter);
    if (!coap_add_data(pdu, sample_counter, data)){
        coap_log_impl(LOG_ERR, "cannot add payload\n");
        goto finish;
    }

    printf("send_voice: payload added\n");

    /* add a Uri-Path option */
    const char *tag = "talk";
    // const char *tag = "voice_data_rcv";
    coap_add_option(pdu, COAP_OPTION_URI_PATH, 5,
                    tag);

    coap_show_pdu(LOG_WARNING, pdu);
    /* and send the PDU */
    if (coap_send(session, pdu) == COAP_INVALID_MID) {
      coap_log_impl(LOG_ERR, "cannot send CoAP pdu\n");
      goto finish;
    }

    printf("going in while...\n");

    // coap_io_process(ctx, COAP_IO_NO_WAIT);

    int retries=30;

    while (have_response == 0 && retries > 0){
      coap_io_process(ctx, COAP_IO_NO_WAIT);
      retries--;
    }

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

int main(){

    //TODO demo
   

    connect_to_coap_server();
    return 0;

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