#include <stdio.h>
#include <string.h>

#include "sdft.h"

#include "wav.h"

int main(int argc, char* argv[])
{
  if (argc < 4)
  {
    return 1;
  }

  const size_t dftsize = atoi(argv[1]);
  const size_t hopsize = atoi(argv[2]);
  const char* srcfile = argv[3];
  const char* wavfile = argv[4];

  sdft_t* sdft = sdft_alloc(dftsize); //sdft_alloc_custom(dftsize, getwindow(window), getlatency(latency));

  float* input;
  size_t size;
  size_t sr;

  if (!readwav(srcfile, &input, &size, &sr))
  {
    return 1;
  }

  printf("C\t%s %zu %zuHz\n", srcfile, size, sr);
  size = (size / hopsize) * hopsize;

  float* output = malloc(size * sizeof(float));
  sdft_double_complex_t* buffer = malloc(hopsize * dftsize * sizeof(sdft_double_complex_t));

  int progress = 0;

  for (int i=0; i< 512; i++){
    printf("%f ", input[i]);
  }
  printf("\n");

  for (size_t i = 0, j = 0; i < size; i+=hopsize, j++)
  {
    const double percent = (i / hopsize + 1.0) / (size / hopsize);

    if ((int)(percent * 10) != progress)
    {
        progress = (int)(percent * 10);
        printf("%i%%\n", progress * 10);
    }
     
    // for (int x=0; x< 5; x++){
    //   printf("%f ", input[i+x]);
    // }
    // printf("\n");

    sdft_sdft_n(sdft, hopsize, input + i, buffer);
    // for (int x=0; x< 5; x++){
    //   printf("(%f, %f) ", buffer[x].r, buffer[x].i);
    // }
    // printf("\n");

    sdft_isdft_n(sdft, hopsize, buffer, output + i);

    // for (int x=0; x< 5; x++){
    //   printf("%f ", output[i+x]);
    // }
    // printf("\n");

  }

  writewav(wavfile, output, size, sr);

  free(buffer);
  free(output);
  free(input);

  sdft_free(sdft);

  return 0;
}