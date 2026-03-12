#include "encoding.h"

//generates a base name for the SenML payload
//used in energy.c
void get_base_name(char *str) {
    snprintf(str, BASE_NAME_LEN, "Sensor:energy:");
}

//converts the SenML structure to a JSON payload string
//used in energy.c
int json_to_payload(senml_encoding* js, char* payload)
{
    char com[MAX_PAYLOAD_LEN] = "";
    // Save the current locale (checks the number format)
    char *current_locale = setlocale(LC_NUMERIC, NULL);
    // Set the locale to "C" to avoid problems with the decimal separator
    setlocale(LC_NUMERIC, "C");

    // create the base name
    sprintf(payload, "[{\"bn\":\"%s\",\"bu\":\"%s\"},", js->base_name, js->base_unit);
    for (int i = 0; i < js->num_measurements; i++)
    {
        sprintf(com, "{\"n\":\"%s\",\"u\":\"%s\",\"t\":\"%s\",\"v\":", 
         js->measurement_data[i].name, 
         js->measurement_data[i].unit, 
         js->measurement_data[i].time);
        strcat(payload, com);
        
        if (js->measurement_data[i].type == V_FLOAT)
        {
            int value = (int)(js->measurement_data[i].v.v * DECIMAL_ACCURACY);
            sprintf(com, "%d}", value);
        }
        else {
            return -1;
        }
        if (i < js->num_measurements - 1)
        {
            strcat(com, ",");
        }
        strcat(payload, com);
    }
    strcat(payload, "]");
    // restore the original locale
    setlocale(LC_NUMERIC, current_locale);
    return strlen(payload);
}

// copies a value from the SenML payload to the output 
//used in parse_str

int copy_value (char *string, char *output, char *start, char *end)
{

  char *start_ptr = strstr(string, start);
  char *end_ptr = strstr(string, end);

  // Check if the start and end pointers are valid
  if (start_ptr == NULL || end_ptr == NULL)
  {
	  return -1;
  }

  // Discard the start part
  start_ptr += strlen(start);
  
  // Copy the value between start and end
  strncpy(output, start_ptr, end_ptr - start_ptr);
  // Add a null terminator to the output string
  output[end_ptr - start_ptr] = '\0';
  // Return the length of the copied value
  return (end_ptr - string) + strlen(end);
}

void parse_str (char *payload, senml_encoding* js)
{
  char* pos = payload;
  int step = 0;

  // copy the base name
  step = copy_value(pos, js->base_name, "\"bn\":", ",");
  // check if the base name was parsed correctly
  if(step == -1)
  {
      LOG_ERR("ERROR: Parsing base name\n");
      LOG_ERR("\t %s\n", pos);
      return;
  }
  // move the position to the end of the base name
  pos += step;
  
  // copy the base unit
  step = copy_value(pos, js->base_unit, "\"bu\":", "}");
  // check if the base unit was parsed correctly
  if(step == -1)
  {
      LOG_ERR("ERROR: Parsing base unit\n");
      return;
  }
  
  // move the position to the end of the base unit
  pos += step;

  // Skip the ",{"
  pos+=2;

  // copy the number of measurements
  for(int i=0; i<js->num_measurements;i++)
  {
      // copy the measurement name
      step = copy_value(pos, js->measurement_data[i].name, "\"n\":", ",");
      // check if the measurement name was parsed correctly
      if(step == -1)
      {
        LOG_ERR("ERROR: Parsing name of measure %d\n", i);
        return;
      }
      // move the position to the end of the measurement name
      pos += step;

      // copy the measurement unit
      step = copy_value(pos, js->measurement_data[i].unit, "\"u\":", ",");
      // check if the measurement unit was parsed correctly
      if(step == -1)
      {
        LOG_ERR("ERROR: Parsing unit of measure %d\n", i);
        return;
      }
     
      // move the position to the end of the measurement unit
      pos += step;

      // copy the measurement time
      step = copy_value(pos, js->measurement_data[i].time, "\"t\":", ",");
      // check if the measurement time was parsed correctly
      if(step == -1)
      {
        LOG_ERR("ERROR: Parsing time of measure %d\n", i);
        return;
      }
     
      // move the position to the end of the measurement time
      pos += step;
      
      // copy the measurement value
      char time_value[MAX_STR_LEN];
      char *endptr;
      step = copy_value(pos, time_value, "\"v\":", "}");
      if(step == -1)
      {
        LOG_ERR("ERROR: Parsing value of measure %d\n", i);
        return;
      }
      // move the position to the end of the measurement value
      pos += step;

      // convert to int
      int value = strtol(time_value, &endptr, 10);

      // check if the conversion was successful 
      if (endptr == time_value) {
         LOG_ERR("ERROR: Convert value of measure %d\n", i);
      }

      // Convert to float
      js->measurement_data[i].v.v = (float)value / (float)DECIMAL_ACCURACY;
      js->measurement_data[i].type = V_FLOAT;
    

      // skip the ",{"
      pos+=2;
  }
}
