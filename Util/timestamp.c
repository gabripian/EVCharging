#include "timestamp.h"

// Advance the time by a certain number of minutes
//used in sensor.c
void advance_time(Timestamp* ts, int minutes)
{
  int hours = 0;
  ts->minute += minutes;
  hours = ts->minute / 60;
  ts->minute = ts->minute % 60;
  ts->hour += hours;

  if (ts->hour >= 24){
    
    ts->hour -= 24;
    ts->day++;

    int days_in_month;
    if (ts->month == 2) {

      days_in_month = 28;

    }else if(ts->month == 4 || ts->month == 6 || ts->month == 9 || ts->month == 11){

      days_in_month = 30;
    }else{

      days_in_month = 31;
    }
    if(ts->day > days_in_month){
      
      ts->day = 1;
      ts->month++;   
      if(ts->month > 12){
        ts->month = 1;
        ts->year++;
      }
    }
  }

  //skip at 7:00
  if(ts->hour >= 2 && ts->hour < 7){
    ts->hour = 7;
    ts->minute = 0;
  }
}
//convert the timestamp into features of the model
//used in energy.c
void convert_to_feature(Timestamp* ts, float* float_ts)
{
  float_ts[0] = ts->month;
  float_ts[1] = ts->day;
  float_ts[2] = hour_decimal(ts->hour, ts->minute, 0);

}

//14:12:08 → 14 + (12/60) + (8/3600) ≈ 14.202
float hour_decimal(int h, int m, int s) {
    return h + (m / 60.0) + (s / 3600.0);
}

//used in energy.c
int timestamp_to_string(Timestamp* ts, char* string)
{
  static char str_month[13];
  static char str_day[13];
  static char str_hour[13];
  static char str_minute[13];
  if (ts->month < 10)
    sprintf(str_month, "0%d", ts->month);
  else
    sprintf(str_month, "%d", ts->month);

  if (ts->day < 10)
    sprintf(str_day, "0%d", ts->day);
  else
    sprintf(str_day, "%d", ts->day);

  if (ts->hour < 10)
    sprintf(str_hour, "0%d", ts->hour);
  else
    sprintf(str_hour, "%d", ts->hour);
  
  if (ts->minute < 10)
    sprintf(str_minute, "0%d", ts->minute);
  else
    sprintf(str_minute, "%d", ts->minute);
  
  sprintf(string, "%d-%s-%sT%s:%sZ", ts->year, str_month, str_day, str_hour, str_minute);
  strcat(string, "\0");
  return strlen(string);
}
//used in sensor.c
void string_to_timestamp(char* string, Timestamp* ts)
{
  int result = sscanf(string, "%d-%d-%dT%d:%dZ", &ts->year, &ts->month, &ts->day, &ts->hour, &ts->minute);
  if (result == 5) {
        LOG_DBG("Parsed successfully\n");
    } else {
        LOG_DBG("Failed to parse the string. Only %d values were read.\n", result);
    }
}
