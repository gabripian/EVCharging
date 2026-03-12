#ifndef TIMESTAMP_H
#define TIMESTAMP_H
#include <stdio.h>
#include <string.h>
#include "sys/log.h"
#define TIMESTAMP_STRING_LEN 18
typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
} Timestamp;
//[+] LOG CONFIGURATION
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO
// Advance the time by the given hours
void advance_time(Timestamp* ts, int minutes);
void convert_to_feature(Timestamp* ts, float* float_ts);
float hour_decimal(int h, int m, int s);
int timestamp_to_string(Timestamp* ts, char* string);
void string_to_timestamp(char* string, Timestamp* ts);

#endif // TIMESTAMP_H