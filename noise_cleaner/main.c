#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <kissfft/kiss_fft.h>

#define CHUNK_SIZE 4096

float computeNoiseFloor(float *buffer, int size) {
    float sum = 0;
    for (int i = 0; i < size; i++) {
        sum += buffer[i] * buffer[i];
    }
    return sqrt(sum / size);
}

// #if 0
void spectralSubtraction(float *input, float *output, int size, float *noiseSpectrum) {
    kiss_fft_cfg cfg = kiss_fft_alloc(size, 0, NULL, NULL);
    kiss_fft_cpx in[size], out[size];

    // FFT
    for (int i = 0; i < size; i++) {
        in[i].r = input[i];
        in[i].i = 0.0;
    }
    kiss_fft(cfg, in, out);

    // Subtract noise spectrum
    for (int i = 0; i < size; i++) {
        float magnitude = sqrt(out[i].r * out[i].r + out[i].i * out[i].i);
        magnitude = fmax(magnitude - noiseSpectrum[i], 0);

        float phase = atan2(out[i].i, out[i].r);
        out[i].r = magnitude * cos(phase);
        out[i].i = magnitude * sin(phase);
    }

    // Inverse FFT
    kiss_fft(cfg, out, in);
    for (int i = 0; i < size; i++) {
        output[i] = in[i].r / size;
    }

    free(cfg);
}
// #endif

int main(){
    const char *filename = "noise.pcm"; 
    FILE *file = fopen(filename, "rb");

    uint8_t noise[1024*95];

    unsigned char buffer[CHUNK_SIZE];
    size_t bytesRead=0, bufferOffset=0;

    while ((bytesRead = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        // Process the chunk of data here
        for (size_t i = 0; i < bytesRead; i++) {
            printf("%02X ", buffer[i]); // Example: Print bytes as hexadecimal
        }
        printf("\n");
        memcpy(noise + bufferOffset, buffer, bytesRead);
        bufferOffset += bytesRead;
    }

    // float noise_val = computeNoiseFloor((float *)noise, sizeof(noise));

    fclose(file);

    /******* cleaning audio ************/

    filename = "audio2clean.pcm"; 
    file = fopen(filename, "rb");

    uint8_t audio2clean[1024*95];

    bytesRead=0, bufferOffset=0;

    while ((bytesRead = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        // Process the chunk of data here
        for (size_t i = 0; i < bytesRead; i++) {
            printf("%02X ", buffer[i]); // Example: Print bytes as hexadecimal
        }
        printf("\n");
        memcpy(audio2clean + bufferOffset, buffer, bytesRead);
        bufferOffset += bytesRead;
    }

    float cleaned_output[1024*95];
    printf("trying spectral subtraction\n");
    spectralSubtraction(audio2clean, cleaned_output, 73000, noise);
    printf("spectral subtraction done\n");


    fclose(file);
    /******* saving output ************/
    filename = "audio_cleaned.pcm"; 
    file = fopen(filename, "wb");
    fwrite((uint8_t*)cleaned_output, 1, 73000, file);

    //TODO ricompila libreria in int16_t
    //TODO converti tutti i uint8_t in int16_t 

    return 0;
}