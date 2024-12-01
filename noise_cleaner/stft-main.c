#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "kiss_fft.h"

#define FRAME_SIZE 1024 // Size of each frame
#define HOP_SIZE 512    // Overlap (e.g., 50%)
#define SAMPLE_RATE 44100

void apply_hamming_window(float *frame, int size) {
    for (int i = 0; i < size; i++) {
        frame[i] *= 0.54 - 0.46 * cos(2 * M_PI * i / (size - 1));
    }
}

void compute_stft(float *input_signal, int signal_length, int frame_size, int hop_size) {
    // Allocate memory for the FFT
    kiss_fft_cfg cfg = kiss_fft_alloc(frame_size, 0, NULL, NULL);

    // Temporary buffers
    float *frame = (float *)calloc(frame_size, sizeof(float));
    kiss_fft_cpx *fft_output = (kiss_fft_cpx *)calloc(frame_size, sizeof(kiss_fft_cpx));

    // Number of frames
    int num_frames = (signal_length - frame_size) / hop_size + 1;

    for (int i = 0; i < num_frames; i++) {
        // Extract frame with overlap
        for (int j = 0; j < frame_size; j++) {
            int index = i * hop_size + j;
            frame[j] = (index < signal_length) ? input_signal[index] : 0.0f;
        }

        // Apply window function
        apply_hamming_window(frame, frame_size);

        // Convert frame to complex input for FFT
        kiss_fft_cpx *fft_input = (kiss_fft_cpx *)calloc(frame_size, sizeof(kiss_fft_cpx));
        for (int j = 0; j < frame_size; j++) {
            fft_input[j].r = frame[j];
            fft_input[j].i = 0.0; // No imaginary part
        }

        // Perform FFT
        kiss_fft(cfg, fft_input, fft_output);

        // Process FFT output
        printf("Frame %d:\n", i + 1);
        for (int k = 0; k < frame_size / 2 + 1; k++) { // Only keep positive frequencies
            float magnitude = sqrt(fft_output[k].r * fft_output[k].r + fft_output[k].i * fft_output[k].i);
            float phase = atan2(fft_output[k].i, fft_output[k].r);
            printf("Freq bin %d: Magnitude = %.2f, Phase = %.2f\n", k, magnitude, phase);
        }

        free(fft_input);
    }

    // Clean up
    free(frame);
    free(fft_output);
    free(cfg);
}

int main() {
    // Example signal: Sine wave + noise
    int signal_length = SAMPLE_RATE * 1; // 1 second of audio
    float *signal = (float *)malloc(signal_length * sizeof(float));

    // Generate a test signal (sine wave at 440 Hz with noise)
    for (int i = 0; i < signal_length; i++) {
        signal[i] = 0.5 * sin(2 * M_PI * 440 * i / SAMPLE_RATE) + 0.05 * ((float)rand() / RAND_MAX - 0.5);
    }

    // Compute STFT
    compute_stft(signal, signal_length, FRAME_SIZE, HOP_SIZE);

    // Clean up
    free(signal);
    return 0;
}

