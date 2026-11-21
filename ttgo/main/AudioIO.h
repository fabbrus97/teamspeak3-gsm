/*
 * Low-level audio I/O for the sample ISR, plus the audio settings.
 * The sample rate itself is AUDIO_SAMPLE_RATE in SETTINGS.h.
 *
 * Kept in a header so the Arduino .ino preprocessor does not try to
 * generate prototypes for these IRAM functions.
 */
#ifndef AUDIOIO_H
#define AUDIOIO_H

#include "SETTINGS.h"
#include "driver/dac.h"
#include "driver/adc.h"
#include "hal/adc_ll.h"
#include "hal/dac_ll.h"

#define AUDIO_TIMER_HZ        8000000          // sample timer tick: 80 MHz APB / 10, must be a multiple of AUDIO_SAMPLE_RATE
#define AUDIO_ADC_CHANNEL     ADC1_CHANNEL_6   // GPIO34, modem speaker output
#define AUDIO_ADC_ATTEN       ADC_ATTEN_DB_12  // the register reset default, i.e. what GPIO34 has always run at
#define AUDIO_FAST_IO         1                // 1: drive ADC/DAC registers directly from the ISR; 0: use the (slow) driver calls
#define AUDIO_STATS           1                // print ISR timing and buffer counters every 5 s during calls
#define AUDIO_PLAY_BUFFER_MS  512              // TS -> phone ring buffer
#define AUDIO_SEND_BUFFER_MS  768              // phone -> TS ring buffer
#define AUDIO_SEND_CHUNK_MS   32               // send to the plugin once this much audio is buffered
#define AUDIO_MAX_PACKET      1024             // max bytes per UDP packet (the plugin reads up to 2000)

static_assert(AUDIO_TIMER_HZ % AUDIO_SAMPLE_RATE == 0, "AUDIO_SAMPLE_RATE must divide AUDIO_TIMER_HZ");

#define COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")

#if AUDIO_STATS
extern volatile uint32_t st_isr_max_cycles, st_adc_not_ready, st_play_empty, st_play_overrun, st_cap_overrun;
#define STAT_ADD(v, n) ((v) = (v) + (n))
#else
#define STAT_ADD(v, n)
#endif

/*
 * DAC output (GPIO25). The fast path is the register write that the driver's
 * dac_output_voltage() does, minus the spinlock and the call into flash.
 */
static inline void IRAM_ATTR audio_dac_write(uint8_t value) {
#if AUDIO_FAST_IO
  dac_ll_update_output_value(DAC_CHAN_0, value);
#else
  dac_output_voltage(DAC_CHAN_0, value);
#endif
}

/*
 * ADC input, 12 bit reduced to 8. The fast path is pipelined: it collects the
 * conversion started at the previous tick (long finished, a tick is >= 62.5 us)
 * and starts the next one, so the ISR never waits for the ADC.
 * setup() configures the ADC once through the driver before this is used.
 */
static inline uint8_t IRAM_ATTR audio_adc_read() {
#if AUDIO_FAST_IO
  static uint8_t last = 128;
  if (adc_oneshot_ll_get_event(ADC_LL_EVENT_ADC1_ONESHOT_DONE)) {
    last = adc_oneshot_ll_get_raw_result(ADC_UNIT_1) >> 4;
  } else {
    STAT_ADD(st_adc_not_ready, 1);
  }
  // non-blocking version of adc_oneshot_ll_start()
  if (HAL_FORCE_READ_U32_REG_FIELD(SENS.sar_slave_addr1, meas_status) == 0) {
    SENS.sar_meas_start1.meas1_start_sar = 0;
    SENS.sar_meas_start1.meas1_start_sar = 1;
  }
  return last;
#else
  return adc1_get_raw(AUDIO_ADC_CHANNEL) >> 4;
#endif
}

#endif
