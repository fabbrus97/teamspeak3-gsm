#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_DEBUG SerialMon
#define TINY_GSM_MODEM_SIM800 // Modem is SIM800L

// Sample rate (Hz) of the audio exchanged with the TeamSpeak plugin, both directions.
// MUST match "sample_rate" in the [audio] section of the plugin's settings.ini.
// 16000 or 8000 (the old value); any rate that divides AUDIO_TIMER_HZ in main.ino works.
#define AUDIO_SAMPLE_RATE 16000

extern int ACCEPT_ONLY_PB;

#endif
