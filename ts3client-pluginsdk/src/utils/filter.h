/*
 * Band-pass filter for the audio coming from the microcontroller.
 *
 * GSM voice only carries ~300-3400 Hz. Everything outside that band in the
 * signal we receive is noise picked up on the board (and the ADC's DC offset),
 * so it is removed before the audio reaches TeamSpeak:
 *  - 2nd order Butterworth high-pass at low_hz
 *  - 4th order Butterworth low-pass at high_hz (two cascaded biquads)
 */
#ifndef FILTER_H
#define FILTER_H

typedef struct {
    float b0, b1, b2, a1, a2; // coefficients, normalized by a0
    float z1, z2;             // state (transposed direct form II)
} biquad_t;

#define BANDPASS_MAX_STAGES 3

typedef struct {
    int stages;
    biquad_t s[BANDPASS_MAX_STAGES];
} bandpass_t;

/* low_hz <= 0 disables the high-pass, high_hz <= 0 disables the low-pass.
 * The low-pass is also skipped if high_hz is too close to rate/2. */
void bandpass_init(bandpass_t* f, int rate, int low_hz, int high_hz);

/* Filters one 16-bit sample (state is kept in f) */
short bandpass_process(bandpass_t* f, short x);

#endif
