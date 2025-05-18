#include "sdft.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_INPUT 102400
#define DFT_BINS 6

size_t pcm_uint8_t_read_and_dft(FILE* file, sdft_fdx_t* output){
    uint8_t input_data[MAX_INPUT];
    size_t input_length = 0;
    int _r = 0;

    printf("Reading file\n");

    while(_r = fread(&(input_data[input_length]), sizeof(uint8_t), 512, file)) input_length += _r;

    size_t n = input_length; // number of samples

    float input_data_f[input_length];

    for (int i=0; i < input_length; i++){
        input_data_f[i] = input_data[i];
        input_data_f[i] = input_data_f[i] * 0.00784313725490196078f;    /* 0..255 to 0..2 */
        input_data_f[i] = input_data_f[i] - 1;
    }

    // float* x = input_data_f; // analysis samples of shape (n)
    float* y = malloc(input_length*sizeof(float)); // synthesis samples of shape (n)

    sdft_t* sdft = sdft_alloc(DFT_BINS); // create sdft plan

    // sdft_float_complex_t* buffer = malloc(HOP_SIZE * m * sizeof(sdft_float_complex_t));

    // output = malloc(input_length * DFT_BINS * sizeof(sdft_fdx_t));

    sdft_sdft_n(sdft, input_length, input_data_f, output); // extract dft matrix from input samples
    printf("rightest (%f, %f)\n", output[180].r, output[180].i);

    // sdft_free(sdft);

    return input_length;
}

void idft_and_float_conversion(sdft_fdx_t* data, size_t length, short* output){
    sdft_t* sdft = sdft_alloc(DFT_BINS); // create sdft plan

    float buffer[length];
    
    data[180].r = 0; data[180].i = 0;

    sdft_isdft_n(sdft, length, data, buffer); // synthesize output samples from dft matrix
    
    int r;
    size_t i;
    for (i = 0; i < length; ++i) {
        float x = buffer[i];
        float c;
        c = ((x < -1) ? -1 : ((x > 1) ? 1 : x));
        c = c + 1;
        r = (int)(c * 32767.5f);
        r = r - 32768;
        output[i] = (short)r;  
    }
}

int main(int argc, char** argv){

    FILE* data_file = fopen(argv[1], "rb");
    sdft_fdx_t* data = malloc(100000 * DFT_BINS *sizeof(sdft_fdx_t));
    size_t data_length = pcm_uint8_t_read_and_dft(data_file, data);
    
    printf("after (%f, %f)\n", data[180].r, data[180].i);

    FILE* noise_file = fopen(argv[2], "rb");
    sdft_fdx_t* noise = malloc(100000 * DFT_BINS *sizeof(sdft_fdx_t));
    int noise_length = pcm_uint8_t_read_and_dft(noise_file, noise);


    float mean_i[noise_length*DFT_BINS]; float mean = 0;

    for (int i=0; i<noise_length*DFT_BINS; i++){
        mean_i[i] = sqrt(noise[i].r*noise[i].r
                        + noise[i].i*noise[i].i);
    }
    
    for (int i = 0; i < noise_length*DFT_BINS; i ++) mean += mean_i[i];
    mean /= noise_length*DFT_BINS;
    printf("noise magnitude is %f\n", mean);

    for (int b=0; b < DFT_BINS; b++){


        for (int i=0; i < data_length; i++){
            float phase = atan2(data[b*data_length + i].i, data[b*data_length + i].r);
            float magn = sqrt(data[b*data_length + i].r*data[b*data_length + i].r
                         + data[b*data_length + i].i*data[b*noise_length + i].i);
            magn = magn - mean > 0 ? magn - mean : 0;

            data[b*data_length + i].r = magn * cosf(phase);
            data[b*data_length + i].i = magn * sinf(phase);
        }
    }
    
    short short_output[data_length];
    idft_and_float_conversion(data, data_length, short_output);

    
    printf("step 6\n");
    FILE* output_file = fopen(argv[3], "wb");
    for (int i=0; i < data_length; i += 512){
        // for (int j=0; j < 512; j++){
        //     printf("%i ", final_output[i+j]);
        // }
        // printf("\n");
        fwrite(&(short_output[i]), sizeof(short), 512, output_file);
        // break;
    }

    printf("step 7\n");



    fclose(output_file);

    // sdft_free(sdft); // destroy sdft plan
    
    return 0;
}