#include <kissfft/kiss_fft.h>
#include <kissfft/kiss_fftr.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "voice.h"
#include <float.h>

float diff = FLT_MAX - FLT_MIN;

void myfft(short * samples, uint8_t* out, int count){
  // uint8_t *data = malloc(count * sizeof(uint8_t));
  kiss_fft_scalar data[count];

  printf("size of uint8_t is %i\n", sizeof(uint8_t));

  for (int i = 0; i < count; i++){
    // uint8_t uint8Value = (uint8_t)((255.0*((samples[i]+32767)/65536.0)));
    // // data[i] = uint8Value;
    data[i] = samples[i];
    printf("conversion %i ok\n", i);
  }
  int is_inverse_fft = 0;
  printf("allocating kiss_fft...\n");
  kiss_fftr_cfg cfg = kiss_fftr_alloc( 48000 ,is_inverse_fft ,NULL,NULL );
  printf("creating the buffer...\n");
  kiss_fft_cpx output[count];
  printf("converting...\n");
  kiss_fftr(cfg, data, output);
  
  printf("filtering...\n");
  for (int i=0; i<24000; i++){
    // printf("whatever r is amounts to %f\n", output[i].r);
    if (output[i].r > 8000) output[i].r = 0;
  }

    is_inverse_fft = 1;
  cfg = kiss_fftr_alloc( 2048 ,is_inverse_fft ,NULL,NULL );
  //   kiss_fftri(cfg, (const kiss_fft_scalar*) output, data);
  kiss_fftri(cfg, output, data);
  kiss_fft_free(cfg);

  printf("experimental part\n");

  for (int i = 0; i < count; i++){
    
    uint8_t uint8Value = 0;
    if (samples[i] >= 0)
        uint8Value = (uint8_t)(255.0*(samples[i]/FLT_MAX));
    else
        uint8Value = (uint8_t)(255.0*(samples[i]/FLT_MIN));
    // // data[i] = uint8Value;
    out[i] = data[i];
  }

}


int main(){
    printf("audio data size is %i\n", sizeof(audio_data));
    uint8_t out[81998];
    myfft(audio_data, out, 81998);


    return 0;
}