#ifndef SIMUTILS_H
#define SIMUTILS_H
#include "SETTINGS.h"
#include <string.h>

#include <HardwareSerial.h>

extern int call_in_progress;
extern int recording_noise;

void parse_serial(const char* str);
int check_caller_in_pb(const char* str);

#endif
