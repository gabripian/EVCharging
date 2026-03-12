#include "contiki.h"
#include "coap-engine.h"
#include "coap-blocking-api.h"
#include "sys/log.h"
#include "etimer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "os/dev/leds.h"
#include "os/dev/button-hal.h"
#include "../Util/timestamp.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define NODE_INFO "{\"node\":\"Sensor\",\"resource\":\"energy\",\"settings\":\"{\\\"sampling_period\\\":15}\"}"


#define SERVER_EP "coap://[fd00::1]:5683" // Server endpoint
#define CREATED_CODE 65 // 2.01 --> Response code for a successful creation of a resource

#define ATTEMPTS 3
int attempts = ATTEMPTS;

#define ATTEMPT_INTERVAL 30
static struct etimer attempt_timer;

extern int sampling_period;
extern int minute_sampling_period;

static const char *service_registration_url = "/registration";      // Registration URL
static const char *service_clock_url = "/clock";                    // clock URL

// It has to be requested to the cloud application
Timestamp timestamp = {
  .year = 0,
  .month = 0,
  .day = 0,
  .hour = 0,
  .minute = 0
};

static struct etimer sleep_timer;

extern coap_resource_t res_energy;        // energy resource
extern coap_resource_t res_settings;      // settings resource for the sampling period

void registration_chunk_handler(coap_message_t *response)
{
    // check if the response is NULL or if the code is not 65 (created)
    if(response == NULL) {
        LOG_ERR("Sensor: Request timed out\n");
    } else if (response->code != 65)
    {
        LOG_ERR("Sensor: Error in registration: %d\n", response->code);
    } else { // Successful registration
        LOG_INFO("Sensor: Successful node registration\n");
        attempts = 0;        
        return;
    }

    // retry registration
    attempts--;

    // if attempts are reached, sleep
    if(attempts == 0) {
        attempts = -1;
    }
}

void clock_chunk_handler(coap_message_t *response)
{
    // check if the response is NULL or if the code is not 69 (success)
    if(response == NULL) {
        LOG_ERR("Request timed out\n");
    }  else if (response->code != 69){

        LOG_ERR("Sensor: Error in registration: %d\n",response->code);

    } else { 
        // Successful clock request
        LOG_INFO("Sensor: Received clock from server: %s\n", response->payload);
        // extract the timestamp from the response
        char str_timestamp[TIMESTAMP_STRING_LEN];
        strncpy(str_timestamp, (char*)response->payload, response->payload_len);
        string_to_timestamp(str_timestamp, &timestamp);
        attempts = 0;   
        return;
    }
    // retry registration
    attempts--;

    // if attempts are reached, sleep
    if(attempts == 0) {
        attempts = -1;
    }

}

PROCESS(sensor, "Sensor");
AUTOSTART_PROCESSES(&sensor);

PROCESS_THREAD(sensor, ev, data) {

    PROCESS_BEGIN();

    coap_activate_resource(&res_energy, "energy");                  // activate the energy resource to be accessed externally

    static coap_endpoint_t server_ep;                               //server endpoint

    static coap_message_t request[1];                               // request used as a pointer (to avoid dynamic memory allocation)
  
    coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);  // populate the coap server endpoint structure

    //clock request
    LOG_INFO("Sensor: Time request process started\n");
    attempts = ATTEMPTS;
    
    while (attempts != 0) {
        LOG_INFO("SEnsor: clock request failed, sleep %d sec and retry...\n", ATTEMPT_INTERVAL);
        // Prepare the GET request: CON --> confirmable (ACK needed), COAP_GET --> GET method, 0 --> initial message ID (will be modified by Contiki)
        coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
        // set the URI path to the clock resource for accessing the server resource
        coap_set_header_uri_path(request, service_clock_url);
        // set empty payload
        coap_set_payload(request, (uint8_t *)"", strlen(""));
        // send coap request and block until response is received or timeout occurs, clock_chunk_handler is the function that will be called when the server responds to the request
        COAP_BLOCKING_REQUEST(&server_ep, request, clock_chunk_handler);

        if (attempts == -1) {
            
            etimer_set(&attempt_timer, (ATTEMPT_INTERVAL * CLOCK_SECOND));
           
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&attempt_timer));
            
            attempts = ATTEMPTS;
        }
    }

    //CoAP server registration
    LOG_INFO("Sensor: Registration process started\n");
    attempts = ATTEMPTS;

    while (attempts != 0) {
        LOG_INFO("Sensor: Registration failed, sleep %d sec and retry...\n", ATTEMPT_INTERVAL);
        // COAP_POST --> POST method, 0
        coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
       
        coap_set_header_uri_path(request, service_registration_url);
        
        coap_set_payload(request, (uint8_t *)NODE_INFO, strlen(NODE_INFO));
        
        COAP_BLOCKING_REQUEST(&server_ep, request, registration_chunk_handler);

        if (attempts == -1) {
            
            etimer_set(&attempt_timer, (ATTEMPT_INTERVAL * CLOCK_SECOND));
          
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&attempt_timer));
           
            attempts = ATTEMPTS;
        }
    }

    //Energy sensing

    LOG_INFO("Sensor Started\n");

    coap_activate_resource(&res_settings, "settings");          // activate the settings resource 
    
    leds_on(LEDS_RED);                                          // sensor is in sleep phase
    
    etimer_set(&sleep_timer, sampling_period);                  // sensing interval

    while(1) {

        PROCESS_YIELD();

       if(etimer_expired(&sleep_timer)){
       
        leds_off(LEDS_RED);
        leds_on(LEDS_GREEN);                                         // sensor is in sensing phase
        
        LOG_INFO("Sensor: sense the energy\n"); 
        res_energy.trigger();                                        //trigger res_event_handler in energy.c
       
        advance_time(&timestamp, minute_sampling_period);            // Update the timestamp of some minutes (if period is of 15 seconds, the timstamp is updated of 15 minutes) to sync with the server
      
        leds_off(LEDS_GREEN);                                        // wait for the next sensing interval
        leds_on(LEDS_RED);                                          
        
        etimer_reset(&sleep_timer);
       }
    }

    PROCESS_END();
}