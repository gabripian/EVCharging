#include "contiki.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys/log.h"
#include "etimer.h"
#include "os/dev/leds.h"
#include "os/dev/button-hal.h"
#include "../Util/timestamp.h"
#include "../Util/encoding.h"
#include "../Util/vehicles.h"
#include "coap-engine.h"
#include "coap-blocking-api.h"
#include "coap-observe-client.h"
#include "coap.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define SIMULATION 0    //used for printing floats 

#define SERVER_EP "coap://[fd00::1]:5683"   // CoAP server address
#define CREATED_CODE 65                     
#define ATTEMPTS 3
int attempts = ATTEMPTS;

static int counter = 0;

//#define RES_ENERGY "energy"

#define ATTEMPT_INTERVAL 30
static struct etimer attempt_timer;

#define NODE_INFO "{\"node\":\"Actuator\",\"resource\":\"status\",\"settings\":\"{\\\"priorities\\\":[-1,-1,-1,-1,-1]}\"}"

//necessary because it must be discovered
static char ip_sensor[40];
static const char *service_registration_url = "/registration"; 

static const char *service_discovery_url = "/discovery";

//observing parameters
static coap_endpoint_t sensor_ep;               // Endpoint sensor node
static coap_observee_t* energy_res;             // observing handler

float predicted_energy = -1;
float current_energy = -1;

EV vehicles[MAX_VEHICLES];          //not static, is referred in other files

extern int priorities[MAX_VEHICLES];    // priorities settings

static struct etimer update_timer;

//resources
extern coap_resource_t res_status;          // status of the staton resource
extern coap_resource_t res_settings;         //settings resource for the priorities

// add a new vehicle
void add_vehicle() {
    for(int i = 0; i < MAX_VEHICLES; i++) {
        // add an EV if the slot is free
        if (vehicles[i].id == -1) {
            vehicles[i].id = i;    //the slot is not free anymore 
            vehicles[i].battery = (rand() % (85 - 15 + 1)) + 15;  // battery between 15% and 85%
            vehicles[i].priority = (rand() % 2);
            priorities[i] = vehicles[i].priority;
            
            printf("Added vehicle %d | Battery: %d%% | Priority: %s\n", i, vehicles[i].battery, vehicles[i].priority ? "High" : "Low");
            return;
        }
    }
    printf("Station full\n");
}

//remove one or more vehicles
void remove_vehicle() {
    int removed = 0;
    for(int i = 0; i < MAX_VEHICLES; i++) {
        // remove all EV that are completely charged
        if ((vehicles[i].id != -1) && vehicles[i].battery >= 100) {
            vehicles[i].id = -1;
            vehicles[i].battery = -1;
            vehicles[i].priority = -1;
            priorities[i] = -1;             // Reset global priorities
            printf("Removed vehicle %d (fully charged)\n", i);
            removed = 1;
        }
    }
    // if no EV is fully charged, remove one at random
    if (!removed) {
        for(int i = 0; i < MAX_VEHICLES; i++) {
            if (vehicles[i].id != -1) {
                vehicles[i].id = -1;
                vehicles[i].battery = -1;
                vehicles[i].priority = -1;
                priorities[i] = -1;             // Reset global priorities
                printf("Removed vehicle %d (random removal)\n", i);
                return;
            }
        }
        printf("Station empty\n");
    }
}

void registration_chunk_handler(coap_message_t *response)
{
    // check if the response is NULL or if the code is not 65 (created)
    if(response == NULL) {
        LOG_ERR("Actuator: Request timed out\n");
    } else if (response->code != 65)
    {
        LOG_ERR("Actuator: Error in registration: %d\n", response->code);
    } else { // Successful registration
        LOG_INFO("Actuator: Successful node registration\n");
        attempts = 0;
        return;
    }
    
    // retry
    attempts--;

    // if the attempts are reached, sleep for some seconds
    if(attempts == 0) {
        attempts = -1;
    }
}

void discovery_chunk_handler(coap_message_t *response)
{
    const uint8_t *buffer = NULL;
    // check if the response is NULL or if the code is not 69 (success)
    if(response == NULL) {
        LOG_ERR("Actuator: Request timed out\n");
    } else if (response->code != 69){
        
        LOG_ERR("Actuator: Error IN DISCOVERY: %d\n", response->code);

    } else { // Successful discovery
        // extract the IP address from the response payload
        coap_get_payload(response, &buffer);
        strncpy(ip_sensor, (char *)buffer, response->payload_len);
        LOG_INFO("Actuator: Successful node discovery: %s\n", ip_sensor);
        attempts = 0;
        return;
    }
    
    // retry
    attempts--;

    // if the attempts are reached, sleep for some seconds
    if(attempts == 0) {
        attempts = -1;
    }
}

static void energy_callback(coap_observee_t *obs, void *notification, coap_notification_flag_t flag)
{
    LOG_INFO("Actuator: ENTERED energy_callback. Flag: %d, Notification ptr: %p\n", flag, notification);
    counter ++;
    printf("------------------------------------------------------ CONTATORE: %d\n", counter);

    if(notification == NULL) {
        LOG_WARN("Actuator: Received NULL notification pointer\n");
        return;
      }
   
    LOG_INFO("Actuator: Notification received: %p\n", notification);

    senml_encoding payload;

    MeasurementData data[2];
    payload.measurement_data = data;
    payload.num_measurements = 2;


    // allocaton for the name and unit
    char base_name[MAX_STR_LEN];
    char base_unit[] = "Kwh";
    char name[2][MAX_STR_LEN];
    char unit[2][MAX_STR_LEN];
    char time[2][TIMESTAMP_STRING_LEN];

    // set the base name and unit
    payload.base_name = base_name;
    payload.base_unit = base_unit;
    
    for (int i = 0; i < 2; i++) {
        payload.measurement_data[i].name = name[i];
        payload.measurement_data[i].unit = unit[i];
        payload.measurement_data[i].time = time[i];
    }
    const uint8_t *buffer = NULL;
    size_t len = 0;

    // check if the notification is NULL
    if(notification){
        
        // extract the payload from the notification
        len = coap_get_payload(notification, &buffer);
        if (buffer == NULL) {
            LOG_INFO("Actuator: Payload is NULL\n");
            return;
            
        }else if(len == 0){
            LOG_INFO("Actuator: Payload is empty\n");
            return;
        }

    } else {
        LOG_INFO("Actuator: Notification is NULL\n");
        return;
    }

    if (len >= MAX_PAYLOAD_LEN) {
        LOG_ERR("Actuator: Payload too large\n");
        return;
    }

    
    printf("Received payload: %.*s\n", (int)len, (char *)buffer);

    
    // check the notification flag
    switch (flag) {
        case NOTIFICATION_OK:
            // parse the payload
            parse_str((char*)buffer, &payload);

            predicted_energy = payload.measurement_data[0].v.v;
            current_energy = payload.measurement_data[1].v.v;

            if (SIMULATION){
                LOG_INFO("Actuator: received sampled energy: %f\n", current_energy);
                LOG_INFO("Actuator: received predicted energy: %f\n", predicted_energy);
            }else{
                LOG_INFO("Actuator: received sampled energy: %d.%02d\n", (int)current_energy, ((int)(current_energy* 100)) % 100);
                LOG_INFO("Actuator: received predicted energy: %d.%02d\n", (int)predicted_energy, ((int)(predicted_energy* 100)) % 100);
            }

            // trigger res_status event 
            res_status.trigger();

            break;

        case OBSERVE_OK:
            LOG_INFO("Actuator: Observe OK\n");
            break;
        case OBSERVE_NOT_SUPPORTED:
            LOG_ERR("Actuator: Observe not supported\n");
            break;
        case ERROR_RESPONSE_CODE:
            LOG_INFO("Actuator: Error response code\n");
            break;
        case NO_REPLY_FROM_SERVER:
            LOG_INFO("Actuator: No reply from server\n");
            break;    
        default:
            LOG_WARN("Actuator: Unexpected notification flag: %d\n", flag);
            break;
    }
}

PROCESS(actuator, "Actuator");
AUTOSTART_PROCESSES(&actuator);

PROCESS_THREAD(actuator, ev, data) {

    button_hal_button_t *btn;

    PROCESS_BEGIN();

    // Inizialize all EV with -1
    for(int i = 0; i < MAX_VEHICLES; i++) {
        vehicles[i].id = -1;
        vehicles[i].battery = -1;
        vehicles[i].priority = -1;
    }

    // activate the actuator status resource
    coap_activate_resource(&res_status ,"status");
  
    // structure to hold the server endpoint
    static coap_endpoint_t server_ep;
    
    // structure to hold the request, used as a pointer (to avoid dynamic memory allocation)
    static coap_message_t request[1]; 

    // populate the coap server endpoint structure
    coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);

    LOG_INFO("Actuator: Registration process started\n");

    //CoAP server registration
    attempts = ATTEMPTS;
    while (attempts != 0) {
        LOG_INFO("Actuator Registration failed, sleep %d sec and retry...\n", ATTEMPT_INTERVAL);
        // prepare the POST request: CON --> confirmable, COAP_POST --> POST method, 0 --> initial message ID
        coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
        // set the URI path to the registration resource for accessing the server resource
        coap_set_header_uri_path(request, service_registration_url);
        // set the payload with the node information
        coap_set_payload(request, (uint8_t *)NODE_INFO, strlen(NODE_INFO));
        // send coap request and block until response is received or timeout occurs, registration_chunk_handler is the function that will be called when the server responds to the request
        COAP_BLOCKING_REQUEST(&server_ep, request, registration_chunk_handler);

        // If the server does not respond, the node sleeps and tries again
        if (attempts == -1) {
            etimer_set(&attempt_timer, (ATTEMPT_INTERVAL * CLOCK_SECOND));
            // Wait for the timer to expire and wake up the process
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&attempt_timer));
            // retstart clock attempts
            attempts = ATTEMPTS;
        }
  }

  //discover the sensor

  LOG_INFO("Actuator: Discovery process started\n");
  
  attempts = ATTEMPTS;
  while (attempts != 0) {

    LOG_INFO("Actuator Discovery failed, sleep %d sec and retry...\n", ATTEMPT_INTERVAL);
    // COAP_GET --> GET method
    coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
    // set the URI path to the clock resource for accessing the server resource
    coap_set_header_uri_path(request, service_discovery_url);
    // set the payload
    coap_set_payload(request, (uint8_t *)"energy", strlen("energy"));
   
    COAP_BLOCKING_REQUEST(&server_ep, request, discovery_chunk_handler);

    if (attempts == -1) {
       
        etimer_set(&attempt_timer, (ATTEMPT_INTERVAL * CLOCK_SECOND));
        
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&attempt_timer));
      
        attempts = ATTEMPTS;
    }

  }
    
  LOG_INFO("Actuator: REgistration and discovery completed\n");
    
    char energy_ep[100];         //register to the energy resource
  
    snprintf(energy_ep, 100, "coap://[%s]:5683", ip_sensor);          // create the energy endpoint string
    
    coap_endpoint_parse(energy_ep, strlen(energy_ep), &sensor_ep);   // parse the load endpoint string

    energy_res = coap_obs_request_registration(&sensor_ep, "energy", energy_callback, NULL);

    LOG_INFO("Actuator started\n");
    
    coap_activate_resource(&res_settings, "settings");          // activate the settings resource

    btn = button_hal_get_by_index(0);                          // returns the button of index 0, since we only have one button

    etimer_set(&update_timer, (CLOCK_SECOND));

    while(1){

        PROCESS_YIELD();

        if(ev == button_hal_release_event){

            btn = (button_hal_button_t *)data;
            //if the button is released after 3 seconds or more, remove one ore more vehicles, otherwise add a vehicle
            if(btn->press_duration_seconds > 3) {
                remove_vehicle();
                res_status.trigger();
            } else {
                add_vehicle();
                res_status.trigger();
            }

        }else if(etimer_expired(&update_timer)){
            //update the vehicles priorities every second, necessary to sincronize with the priority settings by the user
            for (int i = 0; i < MAX_VEHICLES; i++) {
                if (vehicles[i].id != -1 && (vehicles[i].priority != -1 && priorities[i] != -1)) {
                    vehicles[i].priority = priorities[i];
                }
            }
            etimer_reset(&update_timer);

        }
    }
    
    PROCESS_END();
}