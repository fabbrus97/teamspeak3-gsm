#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "../settings.h"
#include "sdft.h"

#define MAX_INPUT 1000000

#define LOADED 1
#define UNLOADED 2
#define NOISE_UNDEF 3
#define RELOAD 4

int delete_noiseprof_file();
size_t pcm_uint8_t_read_and_dft(FILE* file, sdft_fdx_t* output);
void idft_and_float_conversion(sdft_fdx_t* data, size_t length, short* output);
int save_noise(uint8_t* raw_audio, size_t n_sample);
int compute_noise_profile();
int load_noise_profile();
int remove_noise(uint8_t* input, size_t input_length, uint8_t* output);

