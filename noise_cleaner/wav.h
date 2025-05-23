#pragma once

#include <stdbool.h>
#include <stdlib.h>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

float* allocwav(const size_t size)
{
  #if defined(__cplusplus)
    return new float[size];
  #else
    return malloc(size * sizeof(float));
  #endif
}

void freewav(float* data)
{
  #if defined(__cplusplus)
    delete[] data;
  #else
    free(data);
  #endif
}

size_t drwav_write_pcm_frames_f32_to_s16(drwav* wav, const size_t samples, const float* data)
{
  size_t size = samples * wav->channels;
  drwav_int16* data_int16 = (drwav_int16*)malloc(size * sizeof(drwav_int16));

  drwav_f32_to_s16(data_int16, data, size);
  size = (size_t)drwav_write_pcm_frames(wav, samples, data_int16);

  free(data_int16);
  return size;
}

bool readwav(const char* path, float** data, size_t* size, size_t* samplerate)
{
  (*data) = NULL;
  (*size) = 0;
  (*samplerate) = 0;

  drwav wav;

  if (drwav_init_file(&wav, path, NULL) != DRWAV_TRUE)
  {
    return false;
  }

  const size_t samples = (size_t)wav.totalPCMFrameCount;
  const size_t channels = (size_t)wav.channels;

  (*data) = allocwav(samples * channels);
  (*size) = samples * channels;

  if (drwav_read_pcm_frames_f32(&wav, samples, (*data)) != samples)
  {
    drwav_uninit(&wav);

    freewav(*data);

    (*data) = NULL;
    (*size) = 0;

    return false;
  }

  if (channels > 1)
  {
    for (size_t i = 0; i < samples; ++i)
    {
      (*data)[i] = (*data)[i * channels];

      for (size_t j = 1; j < channels; ++j)
      {
        (*data)[i] += (*data)[i * channels + j];
      }

      (*data)[i] /= channels;
    }

    (*size) = samples;
  }

  (*samplerate) = wav.sampleRate;

  drwav_uninit(&wav);

  return true;
}

bool writewav(const char* path, const float* data, const size_t size, const size_t samplerate)
{
  const size_t samples = size;
  const size_t channels = 1;

  drwav wav;
  drwav_data_format format;

  format.container = drwav_container_riff;
  format.format = DR_WAVE_FORMAT_PCM;
  format.bitsPerSample = sizeof(drwav_uint16) * 8;
  format.channels = (drwav_uint16)channels;
  format.sampleRate = (drwav_uint16)samplerate;

  if (drwav_init_file_write(&wav, path, &format, NULL) != DRWAV_TRUE)
  {
    return false;
  }

  if (drwav_write_pcm_frames_f32_to_s16(&wav, samples, data) != samples)
  {
    drwav_uninit(&wav);

    return false;
  }

  drwav_uninit(&wav);

  return true;
}