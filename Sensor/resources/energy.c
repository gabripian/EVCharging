#include "wind_power_prediction.h"
#include "contiki.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coap-engine.h"
#include "sys/log.h"
#include "os/dev/button-hal.h"
#include "../Util/timestamp.h"
#include "../Util/encoding.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uiplib.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define SIMULATION 0    //used for printing floats 

#define MONTH_MEAN 6.52755838f
#define MONTH_STD  3.44633796f

#define DAY_MEAN   15.81850962f
#define DAY_STD    8.76325198f

#define HOUR_MEAN  13.26916638f
#define HOUR_STD   6.7547069f

#define START_THRESHOLD 5.0f      // Threshold that indicates the first sampled value of the energy

#define TURBINE_MAX_KWH 100.0f    // Maximum capacity in kWh/h of the turbine

extern Timestamp timestamp;

extern int sampling_period;

//variable to store the energy predicted in the previous cycle
static float prev_prod = -1.0f;
static float curr_prod = START_THRESHOLD, prediction = 0.0f;

static void res_get_handler(coap_message_t *request, coap_message_t *response,
                          uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler(void);

// observable resource energy
EVENT_RESOURCE(
        res_energy, 
        "title=\"energy\";rt=\"json+senml\";if=\"sensor\";obs", 
        res_get_handler, 
        NULL,
        NULL,
        NULL,
        res_event_handler);

// Uniform random in [–factor, +factor]
static float random_correction(float factor)
{
  return ((float)rand()/RAND_MAX*2.0f - 1.0f) * factor;
}

static void predict_energy(){

  LOG_INFO("Sensor: start energy prediction\n");
   
  float features[] = {0, 0, 0};
    
  printf("%p\n", eml_error_str);                      // This is needed to avoid compiler error (warnings == errors)

  convert_to_feature(&timestamp, features);           //convert timestamp into features

  features[0] = (features[0]- MONTH_MEAN) / MONTH_STD;
  features[1] = (features[1]- DAY_MEAN) / DAY_STD;
  features[2] = (features[2]- HOUR_MEAN) / HOUR_STD;

  //update current prod
  if(prev_prod != -1.0f){
      curr_prod = prev_prod * (1.0f + random_correction(0.10f));
  }
   
  prediction = wind_power_prediction_predict(features, 3);        //energy prediction using ML

  if(SIMULATION){
    printf("Predicted Power [0 - 1]: %f kW\n", prediction);
  }else{
    printf("Predicted Power [0 - 1]: %d.%02d kW\n", (int)prediction, ((int)(prediction* 100)) % 100);
  }
    
  prediction = prediction * TURBINE_MAX_KWH * (15.0f/60.0f);

  if(SIMULATION){
    printf("Predicted Real Power: %f kW\n", prediction);
  }
  else{
    printf("Predicted Real Power: %d.%02d kW\n", (int)prediction, ((int)(prediction* 100)) % 100);

  }
    
  prev_prod = prediction;

}

static void res_get_handler(coap_message_t *request, coap_message_t *response,
                          uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
    
    LOG_INFO("Sensor:--------------NEW-GET-ENERGY-REQUEST---------------------\n");

    
    MeasurementData data[2];
    senml_encoding encoder;

    char names[2][MAX_STR_LEN] = {"predicted", "sampled"};
    char timestamp_str[TIMESTAMP_STRING_LEN];
    char base_name[BASE_NAME_LEN];
    int payload_len = 0;
        
    timestamp_to_string(&timestamp, timestamp_str);

    get_base_name(base_name);              // create the base name
     
    // initialize the values of the resources
    // predicted energy
    data[0].name = names[0];
    data[0].unit = "Kwh";
    data[0].time = timestamp_str;
    data[0].v.v = prediction;
    data[0].type = V_FLOAT;

    //sampled energy
    data[1].name = names[1];
    data[1].unit = "Kwh";
    data[1].time = timestamp_str;
    data[1].v.v = curr_prod;
    data[1].type = V_FLOAT;

    // create the SenML encoding
    encoder.base_name = base_name;
    encoder.base_unit = "Kwh";
    encoder.measurement_data = data;
    encoder.num_measurements = 2;

    payload_len = json_to_payload(&encoder, (char*)buffer);       // convert the JSON to a payload and return the length
   
    // check if the payload is valid
    if (payload_len < 0)
    {
      LOG_ERR("\t Error in the json_to_payload function\n");
      coap_set_status_code(response, INTERNAL_SERVER_ERROR_5_00);
      coap_set_payload(response, buffer, 0);
      return;

    } else if (payload_len > preferred_size) // check if the buffer is big enough
    {
      LOG_ERR("\t Buffer overflow\n");
      coap_set_status_code(response, INTERNAL_SERVER_ERROR_5_00);
      coap_set_payload(response, buffer, 0);
      return;
    }

    // Prepare the response. Set the Content-Format header field to "application/json" to be interpreted as a JSON payload
    coap_set_header_content_format(response, APPLICATION_JSON);
    // Set the ETag header field to the length of the payload (used as content version) --> 1 byte ETag lenght
    coap_set_header_etag(response, (uint8_t *)&payload_len, 1);
    // Set the payload to the response
    coap_set_payload(response, buffer, payload_len);

    LOG_INFO("Sensor: Sending data: %s with size: %d\n", buffer, payload_len);
}

static void res_event_handler(void) {

    char timestamp_str[TIMESTAMP_STRING_LEN];
    timestamp_to_string(&timestamp, timestamp_str);

    LOG_INFO("Sensor -------------------ENERGY TRIGGER EVENT--------------\n");
    LOG_INFO("Sensor: New sample at time %s\n", timestamp_str);

    // sense the energy
    if(SIMULATION){
      LOG_INFO("Sensor: old current energy: %f\n", curr_prod);
    }else{
      LOG_INFO("Sensor: old current energy: %d.%02d\n", (int)curr_prod, ((int)(curr_prod* 100)) % 100);
    }
    
    predict_energy();               //predict the enrgy with ML

    if(SIMULATION){
      LOG_INFO("Sensor: new current energy: %f\n", curr_prod);
    }else{
      LOG_INFO("Sensor: new current energy: %d.%02d\n", (int)curr_prod, ((int)(curr_prod* 100)) % 100);
    }
    
    LOG_INFO("Sensor: Notifing the observers\n");
    coap_notify_observers(&res_energy);
    if(SIMULATION){
      LOG_INFO("Sensor: Current predicted: %.3f, sampled: %.3f", prediction, curr_prod);
    }else{
      LOG_INFO("Sensor: Current predicted: %d.%02d, sampled: %d.%02d", (int)prediction, ((int)(prediction* 100)) % 100, (int)curr_prod, ((int)(curr_prod* 100)) % 100);
    }

}