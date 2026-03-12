#include "contiki.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coap-engine.h"
#include "sys/log.h"
#include "os/dev/button-hal.h"
#include "../Util/timestamp.h"
#include "../Util/encoding.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

int sampling_period = (15 * CLOCK_SECOND); //in seconds

int minute_sampling_period = 15; // 15 minutes default

static void res_get_handler(coap_message_t *request, coap_message_t *response,
                          uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_put_handler(coap_message_t *request, coap_message_t *response,
                          uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

// settings non observable resource
RESOURCE(
        res_settings, 
        "title=\"res_settings\";rt=\"json\";if=\"core.p\"", 
        res_get_handler, 
        NULL,
        res_put_handler,
        NULL);

//called when the resource is requested
static void res_get_handler(coap_message_t *request, coap_message_t *response,
                          uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
    
    LOG_INFO("sampling period:--------------NEW-GET-SETTING-REQUEST---------------------\n");
   
    char json_str[MAX_PAYLOAD_LEN];
    int payload_len = -1;
    int value = minute_sampling_period;
   
    snprintf(json_str, MAX_PAYLOAD_LEN, "{\"sampling period\":%d}", value);        // create the JSON string
    
    payload_len = strlen(json_str);                                                // set the payload length

    // check if the payload length is valid
    if (payload_len < 0)
    {
      LOG_ERR("\t Error in the json creation\n");
      coap_set_status_code(response, INTERNAL_SERVER_ERROR_5_00);
      coap_set_payload(response, buffer, 0);
      return;
    } else if (payload_len > preferred_size) // payload lenght greater than buffer size
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

    LOG_INFO("Sensor: Sending settings: %s with size: %d\n", buffer, payload_len);
}

// called when a PUT request is received
static void res_put_handler(coap_message_t *request, coap_message_t *response,
                          uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    LOG_INFO("sampling period:--------------NEW-PUT-SETTING-REQUEST---------------------\n");

    const uint8_t *payload = NULL;
    int payload_len = coap_get_payload(request, &payload);

    // check if the payload is valid
    if (payload_len < 0)
    {
      LOG_ERR("\t Error in the payload\n");
      coap_set_status_code(response, BAD_REQUEST_4_00);
      coap_set_payload(response, buffer, 0);
      return;
    }   

    LOG_INFO("Sensor: Received settings: %s\n",(char*)payload);

    int value = 0;
    sscanf((char*)payload, "{\"sampling period\":%d}", &value);

    LOG_INFO("Sensor: Received settings value: %d\n",value);
    // get the value from the payload
    minute_sampling_period = value;
    // update the sampling period
    sampling_period = minute_sampling_period * CLOCK_SECOND;

    
    LOG_INFO("Sensor: New settings: minute_sampling_period: %i \n", minute_sampling_period);

    // Prepare the response
    // CHANGED_2_04: The request has succeeded and the resource have been modified as a result of the PUT request.
    coap_set_status_code(response, CHANGED_2_04);
    // Empty payload
    coap_set_payload(response, buffer, 0);
}