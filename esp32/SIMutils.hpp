#ifndef SIMUTILS_H
#define SIMUTILS_H
#include "SETTINGS.h"
#include <string.h>

#include <HardwareSerial.h>

extern int call_in_progress;
// extern SoftwareSerial GPRS;

void parse_serial(char* str);
int check_caller_in_pb(char* str);

#endif
