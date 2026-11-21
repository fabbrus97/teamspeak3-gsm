#include <math.h>
#include "filter.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* RBJ audio EQ cookbook biquads */
static void biquad_design(biquad_t* b, int highpass, double rate, double f0, double q){
    double w0 = 2.0 * M_PI * f0 / rate;
    double cw = cos(w0), alpha = sin(w0) / (2.0 * q);
    double a0 = 1.0 + alpha;
    double b0, b1, b2;
    if (highpass){
        b0 = (1.0 + cw) / 2.0; b1 = -(1.0 + cw); b2 = b0;
    } else {
        b0 = (1.0 - cw) / 2.0; b1 = 1.0 - cw;    b2 = b0;
    }
    b->b0 = (float)(b0 / a0);
    b->b1 = (float)(b1 / a0);
    b->b2 = (float)(b2 / a0);
    b->a1 = (float)(-2.0 * cw / a0);
    b->a2 = (float)((1.0 - alpha) / a0);
    b->z1 = b->z2 = 0.0f;
}

void bandpass_init(bandpass_t* f, int rate, int low_hz, int high_hz){
    f->stages = 0;
    if (low_hz > 0 && low_hz < rate / 2)
        biquad_design(&f->s[f->stages++], 1, rate, low_hz, 0.70710678);   // Butterworth, 2nd order
    if (high_hz > 0 && high_hz < 0.45 * rate){
        biquad_design(&f->s[f->stages++], 0, rate, high_hz, 0.54119610);  // Butterworth, 4th order:
        biquad_design(&f->s[f->stages++], 0, rate, high_hz, 1.30656296);  // two sections
    }
}

short bandpass_process(bandpass_t* f, short x){
    float v = (float)x;
    for (int i = 0; i < f->stages; i++){
        biquad_t* b = &f->s[i];
        float y = b->b0 * v + b->z1;
        b->z1 = b->b1 * v - b->a1 * y + b->z2;
        b->z2 = b->b2 * v - b->a2 * y;
        v = y;
    }
    if (v > 32767.0f) v = 32767.0f;
    if (v < -32768.0f) v = -32768.0f;
    return (short)lrintf(v);
}
