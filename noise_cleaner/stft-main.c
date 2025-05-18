/* many thanks to 
https://download.ni.com/evaluation/pxi/Understanding%20FFTs%20and%20Windowing.pdf
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <kissfft/kiss_fftr.h>
#include "stft.h"
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159
#endif

#define MAX_INPUT 102400

#define FRAME_SIZE 1024 // Size of each frame
#define HOP_SIZE 512    // Overlap (e.g., 50%)
#define SAMPLE_RATE 44100

typedef struct fft_output{
    int frame_n; 
    kiss_fft_cpx freqdata[FRAME_SIZE]; //fft
    kiss_fft_cpx abs_freqdata[FRAME_SIZE];
    kiss_fft_scalar timedata[FRAME_SIZE]; //ifft
    struct fft_output *next; 
} fft_output;

void compute_stft(int input_length, short* input, fft_output* output) {
    kiss_fftr_cfg cfg = kiss_fftr_alloc(FRAME_SIZE, 0, NULL, NULL);
    if (!cfg) {
        perror("FFT configuration failed");
        return;
    }

    int num_frames = (input_length - FRAME_SIZE) / HOP_SIZE + 1;

    fft_output* current_output = output; 

    for (int frame_n = 0; frame_n < num_frames; frame_n++) {
        current_output->frame_n = frame_n + 1;

        kiss_fft_scalar frame[FRAME_SIZE];
        for (int i = 0; i < FRAME_SIZE; i++) {
            frame[i] = input[frame_n * HOP_SIZE + i];
        }

        // Apply Hamming window
        for (int i = 0; i < FRAME_SIZE; i++) {
            frame[i] *= hamming_window[i];
        }

        // Perform FFT
        kiss_fftr(cfg, frame, current_output->freqdata);

        // Prepare for the next frame
        if (frame_n < num_frames - 1) { // Allocate for next frame unless it's the last one
            current_output->next = malloc(sizeof(fft_output));
            if (!current_output->next) {
                perror("Memory allocation failed for FFT output");
                break;
            }
            current_output = current_output->next;
        } else {
            current_output->next = NULL; // Final frame points to NULL
        }
    }

    // Free FFT configuration
    free(cfg);
}

void compute_stft_v1(int input_length, short* input, fft_output* output){
    kiss_fftr_cfg cfg = kiss_fftr_alloc(FRAME_SIZE, 0, NULL, NULL);

    int num_frames = (input_length - FRAME_SIZE) / HOP_SIZE + 1;

    fft_output head = *output; 

    for (int frame_n = 0; frame_n < num_frames; frame_n++){
        output->frame_n = frame_n+1;
        // output->freqdata = malloc(sizeof(kiss_fft_cpx));
    
        kiss_fft_scalar frame[FRAME_SIZE];
        for(int i=0; i<FRAME_SIZE;i++) frame[i] = input[frame_n+i];

        /* Windowing with Hamming window (prevents discontinuities, thus spectral leakage)
        * Copied from https://github.com/kichiki/WaoN/blob/master/fft.c
        */
        for (int i = 0; i < FRAME_SIZE; i++) {
            frame[i] *= hamming_window[i];//(0.54 - 0.46 * cos(2 * M_PI * i / (FRAME_SIZE - 1)));
        }

        kiss_fftr(cfg, frame, output->freqdata);

        output->next = malloc(sizeof(fft_output));
        output = output->next; 
    }
    output->next == NULL;

    *output = head;

}

void compute_stft_chatgpt(float *input_signal, int signal_length, int frame_size, int hop_size) {
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
        // apply_hamming_window(frame, frame_size);

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

/*
void compute_istft(int num_frames, fft_output* input, kiss_fft_scalar* output) {

    kiss_fftr_cfg ifft_cfg = kiss_fftr_alloc(FRAME_SIZE, 1, NULL, NULL); // 1 for inverse FFT
    kiss_fft_scalar* frame = (kiss_fft_scalar *)malloc(FRAME_SIZE * sizeof(kiss_fft_scalar));

    fft_output head = *input;

    for (int frame = 0; frame < num_frames; frame++) {
        // Perform inverse FFT
        kiss_fft(ifft_cfg, input->freqdata, frame);

        // Overlap-add with windowing
        for (int n = 0; n < FRAME_SIZE; n++) {
            float value = input->freqdata[n].r / (float)FRAME_SIZE; // Normalize by frame size
            value *= window[n];                              // Apply window
            output[frame * hop_size + n] += value;           // Overlap-add
        }
    }

    // Free IFFT configuration
    free(ifft_cfg);
    // free(time_domain_frame);
} */

void istft(fft_output *audio_stft, int n_frames, void *window_function, short* output){
    //thanks to https://github.com/zafarrafii/Zaf-Python/blob/master/zaf.py

    // # Compute the number of samples for the signal
    int number_samples = n_frames * HOP_SIZE + (FRAME_SIZE - HOP_SIZE);
    printf("[ISTFT] number of samples is %i\n", number_samples);


    // # Compute the inverse Fourier transform of the frames and take the real part to ensure real values
    // audio_stft = np.real(np.fft.ifft(audio_stft, axis=0))
    kiss_fftr_cfg ifft_cfg = kiss_fftr_alloc(FRAME_SIZE, 1, NULL, NULL); // 1 for inverse FFT
    kiss_fft_scalar* frame = (kiss_fft_scalar *)malloc(FRAME_SIZE * sizeof(kiss_fft_scalar));

    
    fft_output head = *audio_stft;
    while (audio_stft->next != NULL){
        kiss_fftri(ifft_cfg, audio_stft->abs_freqdata, audio_stft->timedata);

        audio_stft = audio_stft->next;
    }

    //TODO reset audio_stft
    audio_stft = &head; 

    // # Loop over the time frames
    int counter = 0;
    int i = 0;
    for (int j=0; j < n_frames; j++){
        // printf("istft: computing inverse fft on frame %i\n", audio_stft->frame_n);
        
    //     # Perform a constant overlap-add (COLA) of the signal (with proper window function and step length)
        for (int l=0; l < FRAME_SIZE; l++){
            // if (audio_stft->timedata[l].i != 0){
            //     printf("Warning! imaginary part =/= 0: %f\n", audio_stft->timedata[l].i);
            // }
            output[l+i] += audio_stft->timedata[l];
        }
        i = i + HOP_SIZE;
        audio_stft = audio_stft->next;
    }

    audio_stft = &head; 

    for(int i=0; i < number_samples; i++) output[i] /= 1.08; // 1.08 = sum(window_function[0:FRAME_SIZE:HOP_SIZE])


    // # Normalize the signal by the gain introduced by the COLA (if any)
    // audio_signal = audio_signal / sum(window_function[0:window_length:step_length])

    // return audio_signal
}

void inverse_stft(int num_frames, fft_output* input, int output_length, short* output) {
    kiss_fftr_cfg cfg = kiss_fftr_alloc(FRAME_SIZE, 1, NULL, NULL); // Inverse FFT setup
    if (!cfg) {
        perror("FFT configuration for inverse failed");
        return;
    }

    // Clear output buffer
    for (int i = 0; i < output_length; i++) {
        output[i] = 0;
    }

    // Overlap-add buffer
    float overlap_add_buffer[output_length];
    for (int i = 0; i < output_length; i++) {
        overlap_add_buffer[i] = 0.0f;
    }

    // Window normalization buffer (to normalize overlapping frames)
    float normalization_buffer[output_length];
    for (int i = 0; i < output_length; i++) {
        normalization_buffer[i] = 0.0f;
    }

    // Iterate through frames
    fft_output* current_frame = input;
    for (int frame_n = 0; frame_n < num_frames; frame_n++) {
        // Ensure current frame is valid
        if (!current_frame) break;

        // Inverse FFT
        kiss_fft_scalar time_data[FRAME_SIZE];
        kiss_fftri(cfg, current_frame->freqdata, time_data);

        // Multiply by Hamming window and overlap-add
        printf("Multiply by Hamming window and overlap-add for frame %i\n", current_frame->frame_n);
        int start_index = frame_n * HOP_SIZE;
        for (int i = 0; i < FRAME_SIZE; i++) {
            if (start_index + i < output_length) {
                // float windowed_sample = time_data[i] * hamming_window[i];
                overlap_add_buffer[start_index + i] += time_data[i]; //windowed_sample;
                normalization_buffer[start_index + i] += hamming_window[i];
            }
        }

        // Move to the next frame
        current_frame = current_frame->next;
    }

    // Normalize the overlap-add buffer and cast to output
    for (int i = 0; i < output_length; i++) {

        if (normalization_buffer[i] > 0) {
            overlap_add_buffer[i] /= normalization_buffer[i];
        }
        output[i] = (short)overlap_add_buffer[i]; // Convert to 16-bit integer
        // if (output[i] != 0)
        //     printf("[DEBUG] output[i] = %i\n", output[i]);
    }

    // Free FFT configuration
    free(cfg);
}

int main(int argc, char** argv) {
    /* algorithm:
        1. [DONE] read input and compute length 
        2. s     = stft(input)
        3. ss    = abs(s)
        (repeat per noise)
        4. angle = angle(s)
        5. b     = exp(1.0j * angle)
        6. m     = mean(noise_ss)
        7. sa    = ss - m.reshape((m.shape[0],1))
        8. sa0   = sa * b
        9. y     = istft(sa0)
        10. write y
        11. FREE EVERYTHING!!!
    */
    
    //1. 

    if (argc != 4){
        printf("Usage: %s input input_noise output\n", argv[0]);
        exit(1);
    }

    printf("Opening files\n");

    FILE* input = fopen(argv[1], "rb");
    FILE* noise = fopen(argv[2], "rb");

    short input_data[MAX_INPUT];
    short noise_data[MAX_INPUT];
    size_t input_length = 0, noise_length = 0;
    int _r = 0;

    printf("Reading files\n");

    while(_r = fread(&(input_data[input_length]), sizeof(short), 512, input)) input_length += _r;
    //TODO: converti dati demo in 16 bit
    while(_r = fread(&(noise_data[noise_length]), sizeof(short), 512, noise)) noise_length += _r;

    printf("input length is %likb, noise length is %likb\n", input_length/512, noise_length/512);
    int num_frames_data = (input_length - FRAME_SIZE) / HOP_SIZE + 1;
    int num_frames_ns = (noise_length - FRAME_SIZE) / HOP_SIZE + 1;

    // 2.

    fft_output *output_data = malloc(sizeof(fft_output));
    printf("[DEBUG][STFT] first frame number is %i\n", output_data->frame_n);

    compute_stft(input_length, input_data, output_data);
    // printf("Number of frames for input should be %li, checking\n", (input_length - 1024) / 512 + 1);
    // while (output_data->next != NULL){
    //     printf("Frame %i ok\n", output_data->frame_n);
    //     output_data = output_data->next;
    // }

    //3. 

    fft_output* frame = output_data; 

    // while (frame->next != NULL){
    //     // printf("Frame %i ok\n", frame->frame_n);
    //     for (int i=0; i < FRAME_SIZE; i++){
    //         frame->abs_freqdata[i].r = frame->freqdata[i].r < 0 ? -frame->freqdata[i].r : frame->freqdata[i].r;
    //     }
    //     frame = frame->next;
    // }

    //same steps for noise 
    //2. 

    // fft_output *output_noise = malloc(sizeof(fft_output));
    // compute_stft(noise_length, noise_data, output_noise);
    
    //3.

    // frame = output_noise; 

    // while (frame->next != NULL){
    //     // printf("elaboratig frame %i of noise\n", frame->frame_n);
    //     for (int i=0; i < FRAME_SIZE; i++){
    //         frame->abs_freqdata[i].r = frame->freqdata[i].r < 0 ? -frame->freqdata[i].r : frame->freqdata[i].r;
    //         frame->abs_freqdata[i].i = 0;
    //     }
    //     frame = frame->next;
    // }

    //4.
    //5.
    //6.
    // printf("step 6\n");

    // frame = output_noise; 
    // kiss_fft_scalar mean[num_frames_ns];

    // while (frame->next != NULL){
    //     // printf("elaboratig frame %i of %i\n", frame->frame_n, num_frames_ns);
    //     for (int i=0; i < FRAME_SIZE; i++){
    //         mean[frame->frame_n-1] += frame->abs_freqdata[i].r;
    //     }
    //     mean[frame->frame_n-1] /= (float)FRAME_SIZE;
    //     frame = frame->next;
    // }

    //7. subtraction

    // printf("step 7\n");

    // frame = output_data; 

    // while (frame->next != NULL){
    //     for (int i=0; i < FRAME_SIZE; i++){
    //         frame->abs_freqdata[i].r -= mean[frame->frame_n-1];
    //     }
    //     frame = frame->next;
    // }

    //8.
    //9. istft

    frame = output_data; 

    printf("[DEBUG][ISTFT] first frame number is %i\n", frame->frame_n);

    short final_output[input_length];
    printf("[MAIN] number of samples is %i\n", input_length);
    for (int i=0; i < input_length; i++) final_output[i] = 0;
    // istft(frame, num_frames_data, NULL, final_output);
    inverse_stft(num_frames_data, frame, input_length, final_output);

    FILE* output_file = fopen(argv[3], "wb");
    // while (output_data->next != NULL){
    //     fwrite(output_data->timedata, sizeof(short), FRAME_SIZE, output_file);
    //     output_data = output_data->next;
    // }   
    for (int i=0; i < input_length; i += 512){
        // for (int j=0; j < 512; j++){
        //     printf("%i ", final_output[i+j]);
        // }
        // printf("\n");
        fwrite(&(final_output[i]), sizeof(short), 512, output_file);
        // break;
    }
    fclose(output_file);

    

    //10. write
    //11. free!


    // Compute STFT
    // compute_stft(signal, signal_length, FRAME_SIZE, HOP_SIZE);

    // Clean up
    // free(signal);
    return 0;
}

