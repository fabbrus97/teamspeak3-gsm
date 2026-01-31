#include "server.h"
#include "ts3_functions.h"
#include "clog.h"
#define UDP_SIZE 512


sem_t sem_voice_buffer;
int socket_desc;
struct sockaddr_in server_addr, client_addr;
char /* server_message[2000], */ client_message[2000];
socklen_t client_struct_length; // = sizeof(client_addr);

int start_udp_socket(){

    // Clean buffers:
    // memset(server_message, '\0', sizeof(server_message));
    client_struct_length = sizeof(client_addr);
    memset(client_message, '\0', sizeof(client_message));

    // Create UDP socket:
    socket_desc = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(socket_desc < 0){
        ERROR("Error while creating socket");
        return -1;
    }
    INFO("Socket created successfully");

    DEBUG("LISTENING ON PORT %i ON ADDRESS %s", ts_audio_port, ts_ip_bind);

    // Set port and IP:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ts_audio_port);
    server_addr.sin_addr.s_addr = inet_addr(ts_ip_bind);

    // Bind to the set port and IP:
    if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        ERROR("Couldn't bind to the port");
        return -1;
    }
    INFO("Done with binding");

    INFO("Listening for incoming messages...");
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

int send_voice(short* samples, int sample_cnt){

    // IP address and port
    // Initialize sockaddr_in struct
    struct sockaddr_in test_client_addr;
    memset(&test_client_addr, 0, sizeof(test_client_addr)); // Clear the struct
    test_client_addr.sin_family = AF_INET; // IPv4
    test_client_addr.sin_port = htons(ucontroller_audio_port); // Port in network byte order
    DEBUG("SENDING AUDIO TO %s:%i", ucontroller_address, ucontroller_audio_port);
    inet_pton(AF_INET, ucontroller_address, &test_client_addr.sin_addr); // Convert IP address to binary

    size_t const obuf_size = (size_t)(sample_cnt*OSIZE/ISIZE + .5);

    short downsampled[obuf_size];
    size_t odone;
    soxr_error_t error;

    error = soxr_oneshot(ISIZE, OSIZE, 1, /* Rates and # of chans. */
      samples, sample_cnt, NULL,                              /* Input. */
      downsampled, obuf_size, &odone,                             /* Output. */
      &soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I), NULL, NULL);                             /* Default configuration.*/
    
    if (error)
        ERROR("there was an error during downsampling");
    
    if (odone == 0){
        DEBUG("downsample: odone is 0");
        return 1;
    }

    uint8_t data[odone];
    
    convert_short_to_uint8(downsampled, odone, data);

    size_t sent = 0;
    while (sent < odone){ 
        int available = odone - sent > UDP_SIZE ? UDP_SIZE : odone - sent;
        uint8_t tmp[available];
        memcpy(tmp, &(data[sent]), available); 

        if (sendto(socket_desc, tmp, available, 0,
                    (const struct sockaddr_in*)&test_client_addr, sizeof(test_client_addr)) < 0)
        {
            ERROR("Error in sendto(): %s", strerror(errno));
            return 1;
        }

        sent += available;
    }

    return 0;
}


ssize_t receive_data(uint8_t** data){
    ssize_t recvBytes = recvfrom(socket_desc, client_message, sizeof(client_message), 0,
            (struct sockaddr*)&client_addr, &client_struct_length);
    if (recvBytes <= 0){
        // DEBUG("Couldn't receive or no data"); // Commented out to avoid spamming the log
    } else {
        *data = malloc(sizeof(uint8_t)*recvBytes);
        memcpy(*data, client_message, recvBytes);
    }
    return recvBytes;
}


