#include "server.h"
#include "ts3_functions.h"
#define UDP_SIZE 60


// char* server_address = "127.0.0.1";
char* server_address = "192.168.1.20";
char* server_port = "5683";

static unsigned int token_obs = 0;
sem_t sem_voice_buffer;
static int have_response = 0;
static short* voice_buffer;
static int sample_counter = 0;
int socket_desc;
struct sockaddr_in server_addr, client_addr;
char /* server_message[2000], */ client_message[2000];
int client_struct_length; // = sizeof(client_addr);

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

// void convert_short_to_uint8(short * samples, int count, uint8_t* out){
//   uint8_t uint8Value = 0;
//   for (int i = 0; i < count; i++){
//     if (samples[i] < 0) uint8Value = (uint8_t)(((127.0)*(1 - (samples[i] / -32767.0))));
//     else uint8Value = (uint8_t)((127*(samples[i]/32767.0)) + 128); ///65536.0)));
//     out[i] = uint8Value;
//   }
// }

//normalizes short (range -32767, +32767) to uint8_t (range 0, 255)
void convert_short_to_uint8(short * samples, int count, uint8_t* out){
  short tmp = 0;
  for (int i = 0; i < count; i++){
    tmp = samples[i] >> 8;
    tmp += 0x80;
    out[i] = tmp & 0xFF;
 }
}

void convert_short_to_uint8_old(short * samples, int count, uint8_t* out){
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

    // IP address and port
    const char* ip_address = "192.168.1.6";
    int port = 6000;

    float fsamples[sample_counter];
    for (int i = 0; i < sample_counter; i++){
        fsamples[i] = (float)samples[i];
    }

    // Initialize sockaddr_in struct
    struct sockaddr_in test_client_addr;
    memset(&test_client_addr, 0, sizeof(test_client_addr)); // Clear the struct
    test_client_addr.sin_family = AF_INET; // IPv4
    test_client_addr.sin_port = htons(port); // Port in network byte order
    inet_pton(AF_INET, ip_address, &test_client_addr.sin_addr); // Convert IP address to binary

    // if (client_addr == NULL) TODO
    //   return 0;

    // printf("input data (48khz): ");
    // for (int i = 0; i < sample_counter; i++){
    //     printf("%i,", samples[i]);
    // }
    // printf("\n");


    // sample_counter = sample_counter > 100 ? 100 : sample_counter; //TODO
    int obuf_size = (size_t)(sample_counter * OSIZE / ISIZE + .5);
    short downsampled[obuf_size];
    uint8_t tmp[UDP_SIZE];
    //0 means the semaphore is shared between threads
    //1 is the initial value of the semaphore
    // sem_init(&sem_voice_buffer, 0, 1);

    printf("send_voice: trying to add the payload\n");
    
    printf("downsampling discarding a sample every n\n");
    for (int i = 0, j=0; i < sample_counter; i+=ISIZE/OSIZE, j++ ){
        downsampled[j] = samples[i];
    }

    // printf("downsampling using soxr\n");

    ssize_t *idone = malloc(sizeof(ssize_t)), *odone = malloc(sizeof(ssize_t));
    soxr_error_t error;

    // Read from input file

    printf("output buffer has size %i\n", obuf_size);

    // soxr_io_spec_t ioSpec = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
    // soxr_t resampler = soxr_create(ISIZE, OSIZE, 2, NULL, &ioSpec, NULL, NULL);
    // Resample the input buffer
    // error = soxr_process(resampler, samples, sample_counter, idone, downsampled, obuf_size, odone);
    // error = soxr_oneshot(ISIZE, OSIZE, 1, /* Rates and # of chans. */
    //   fsamples, sample_counter, NULL,                              /* Input. */
    //   downsampled, obuf_size, odone,                             /* Output. */
    //   NULL, NULL, NULL);                             /* Default configuration.*/

    if (error) {
        fprintf(stderr, "Error during resampling: %s\n", soxr_strerror(error));
        return 1;
    } 
    
    // *odone = obuf_size;

    // printf("output data (8khz): ");
    // for (int i = 0; i < obuf_size; i++){
    //     printf("%i,", downsampled[i]);
    // }
    // printf("\n"); 
    
    // printf("total size to send: %i expected send: %i\n", (*odone), ((*odone))/UDP_SIZE);

    printf("converting to uint8_t\n");

    // uint8_t data[(*odone)];
    uint8_t data[obuf_size];

    // convert_short_to_uint8(downsampled, *odone, data);
    convert_short_to_uint8(downsampled, obuf_size, data);

    printf("done, sending...\n");

    int sent = 0;
    int mycounter = 1;
    while (sent < obuf_size){ //(*odone)*2){
        struct timeval tv1, tv2;
        gettimeofday(&tv1, NULL);
        long long nanoseconds1 = ((long long)tv1.tv_sec * 1000000LL + (long long)tv1.tv_usec)*1000;

        //int available = (*odone) - sent > UDP_SIZE ? UDP_SIZE : (*odone) - sent;
        int available = obuf_size - sent > UDP_SIZE ? UDP_SIZE : obuf_size - sent;
        memcpy(tmp, data, available);

        // printf("send %i first 10 bytesa are: ", mycounter);
        // for (int i = 0; i<10; i++) printf("%i ", tmp[i]);
        // printf("\n");
        mycounter += 1;

        // if (sendto(socket_desc, data, (*odone), 0,
        if (sendto(socket_desc, data, obuf_size, 0,
                    (const struct sockaddr_in*)&test_client_addr, sizeof(test_client_addr)) < 0)
        {
            fprintf(stderr, "Error in sendto()\n");
            return 1;
        }



        // printf("trying to reset tmp...\n");
        memset(tmp, 0, UDP_SIZE);
        // printf("done\n");

        sent += available;

        gettimeofday(&tv2, NULL);
        long long nanoseconds2 = ((long long)tv2.tv_sec * 1000000LL + (long long)tv2.tv_usec)*1000;

        struct timespec myts;
        myts.tv_sec=0;
        myts.tv_nsec = UDP_SIZE*(10e9)/8000 - (nanoseconds2 - nanoseconds1);
        nanosleep(&myts, &myts);
    }

    // printf("I'm out of for, data sent\n");

    // soxr_delete(resampler); //TODO 


    return 0;
}

int receive_data(char** data, size_t * len){
    struct timespec read_timeout;
    read_timeout.tv_sec=0;
    read_timeout.tv_nsec = 10; //0.1 millisec
    // printf("setting socket option\n");
    // setsockopt(socket_desc, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
    // printf("done\n");
    ssize_t recvBytes = recvfrom(socket_desc, client_message, sizeof(client_message), 0,
            (struct sockaddr*)&client_addr, &client_struct_length);
    if (recvBytes <= 0){
        // printf("Couldn't receive or no data\n");
        // nanosleep(&myts, &myts);
        return -1;
    } else {
        // printf("got data from socket, bytes: %li\n", recvBytes);
    }
    *len = recvBytes-1;
    client_message[recvBytes] = '\0';
    *data = malloc(sizeof(char)*recvBytes);
    memcpy(*data, &client_message[1], recvBytes-1);
    printf("DEBUG i have received the following %i bytes: ", recvBytes);
    for (int i=0; i < recvBytes; i++){
        printf("%i,", client_message[i]);
    }
    printf("\n");
    printf("DEBUG i have copied data and now buffer is: ");
    for (int i=0; i < recvBytes; i++){
        printf("%i,", (*data)[i]);
    }
    printf("\n");

    if (data == NULL)
        printf("data: aw shit here we go again\n");
    else
        printf("data: ok\n");

    return client_message[0];
}

void receive_and_play_voice(void* args){
    // Receive client's message:
    struct timespec myts;
    myts.tv_sec=0;
    myts.tv_nsec = 1000; //10 millisec
    size_t recvBytes = 0;
    for(;;){
        recvBytes = recvfrom(socket_desc, client_message, sizeof(client_message), 0,
            (struct sockaddr*)&client_addr, &client_struct_length);
        if (recvBytes < 0){
            printf("Couldn't receive\n");
            // return -1;
            myts.tv_sec=1;
            nanosleep(&myts, &myts);
            myts.tv_sec=0;
            continue;
        }
        printf("Received message from IP: %s and port: %i\n",
              inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        printf("message length is %i\n", recvBytes);
        short buffer[recvBytes];

        /*for (int i=0; i < recvBytes-1; i+=2){
          uint8_t uint1 = client_message[i+1];
          uint8_t uint2 = client_message[i];
          short short1 = (((short)uint1) << 8);
          short short2 = ((short)uint2);
          buffer[i/2] = short1 + short2;
          // if (i<10){
          //   printf("first byte is %i -> %i, second is %i -> %i, sum is %i\n", uint1, short1, uint2, short2, buffer[i/2]);
          // }
        }*/
        for (int i = 0; i < recvBytes; i++){
          uint8_t uintVal = client_message[i];


          short shortVal = (short) (uintVal - 0x80) << 8; //uintVal < 127 ? (1 - uintVal/(127))*(-32768) : ((uintVal - 127)/127)*(32768);
          buffer[i] = shortVal;
        }

        (*((const struct TS3Functions *)args)).processCustomCaptureData("ts3callbotplayback", buffer, recvBytes);

        nanosleep(&myts, &myts);

        memset(client_message, 0, sizeof(client_message));
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

