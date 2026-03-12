#include "contiki.h"
#include "coap-engine.h"
#include "sys/log.h"
#include "os/dev/leds.h"
#include <stdio.h>
#include <stdlib.h>
#include "../Util/encoding.h"
#include "../Util/timestamp.h"
#include "../Util/vehicles.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

// 15% battery gain per 15'
#define CHARGE_STEP     15.0f
// each EV has a battery of 10 Kwh
#define BATTERY_CAPACITY_KWH 10.0f

//observed values
extern float predicted_energy;    // predicted energy from the sensor
extern float current_energy;      // predicted energy from the sensor

extern EV vehicles[MAX_VEHICLES];

static int scheduled[MAX_VEHICLES];                             //used to order the EV by the priority, contains only ids
static int charging_slots[MAX_VEHICLES] = {0, 0, 0 ,0 ,0};      // 1 if the EV in the slot i is charging, 0 otherwise

static int status[MAX_VEHICLES];            //real status

static float residual = 0.0f;               //residual energy after sheduling

static void schedule();
void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
                          
static void res_event_handler(void);

// observable resource “status”
EVENT_RESOURCE(
        res_status, 
        "title=\"status\";rt=\"json\";if=\"actuator\";obs", 
        res_get_handler, 
        NULL,
        NULL,
        NULL,
        res_event_handler); 

// schedule the charge of the EV based on the current and predicted energy
static void schedule(){

    LOG_INFO("Scheduling process started\n");

    int tmp;                        //used for swaps
    int active_count = 0;           //number of slots with an EV connected
    bool high_priority = false;     

    for (int i = 0; i < MAX_VEHICLES - 1; i++) {
        //consider only slot with a non fully charged EV 
        if (vehicles[i].id != -1 && vehicles[i].battery < 100) {

            scheduled[active_count++] = vehicles[i].id;
            if (vehicles[i].priority == 1) {
                //there are high priority vehicles
                high_priority = true;
            }
        }else if(vehicles[i].id == -1){
            charging_slots[i] = 0;             //if a vehicle has been removed is not charging anymore
        }
       
    }

    // Sort requests by:
    //   1) priority descending (High before Low)
    //   2) battery ascending (lower battery first)
    for (int i = 0; i < active_count - 1; i++) {
        for (int j = i + 1; j < active_count; j++) {

             int id_i = scheduled[i];
             int id_j = scheduled[j];

            // swap if j has higher priority, or same priority but lower battery
            if ((vehicles[id_j].priority > vehicles[id_i].priority) || (vehicles[id_j].priority == vehicles[id_i].priority && vehicles[id_j].battery < vehicles[id_i].battery)) {
                tmp = scheduled[i];
                scheduled[i] = scheduled[j];
                scheduled[j] = tmp;
            }
        }
    }

    // allocate energy in sorted order until it runs out
    residual += current_energy;

    // necessary energy for one charge step (1 kWh if CHARGE_STEP = 10)
    float charge_step_kwh = (CHARGE_STEP / 100.0f) * BATTERY_CAPACITY_KWH;

    for (int i = 0; i < active_count; i++) {
        int id = scheduled[i];
       
         if(charging_slots[id] == 1){
            vehicles[id].battery += CHARGE_STEP;
            if(vehicles[id].battery>100){
                vehicles[id].battery=100;
                charging_slots[id] = 0; //the slot is not in charge anymore
                continue; //if the battery is 100, don't consider the EV
            }

        }
       
        // if enough residual energy, accept the charge
        if (vehicles[id].battery < 100) {
            //if the EV has High priority
            if (vehicles[id].priority == 1) {
                //if there is enough current energy
                if (residual >= charge_step_kwh) {
                    charging_slots[id] = 1;
                    residual -= charge_step_kwh;
                }else {
                    //if there is not enough current energy
                    charging_slots[id] = 0;
                }
            } else {
                // low priority EV
                if(!high_priority){
                    // sif there are only low priority EV, don't postpone charge if there is available energy
                    if (residual >= charge_step_kwh) {
                        charging_slots[id] = 1;
                        residual -= charge_step_kwh;
                    } else {
                        charging_slots[id] = 0;
                    }

                }else{
                    //if there are high priority EV
                    if (predicted_energy >= charge_step_kwh) {
                        charging_slots[id] = 0;  //postpone the charge of ow priority EV
                    } else if (residual >= charge_step_kwh) {
                        charging_slots[id] = 1;
                        residual -= charge_step_kwh;
                    } else {
                        charging_slots[id] = 0;
                    }
                }
            }
        }
    }

    printf("Residual energy: %d.%02d kWh\n", (int)residual, ((int)(residual* 100)) % 100);

    printf("Charging slots: [");
    for (int i = 0; i < MAX_VEHICLES; i++) {
        status[i] = charging_slots[i];
        printf("%d", charging_slots[i]);
        if (i < MAX_VEHICLES - 1) printf(", ");

    }
    printf("]\n");

    coap_notify_observers(&res_status);
}

// This function is called when the resource is requested
void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {

    int payload_len = snprintf((char *)buffer, preferred_size, "{\"vehicles\":[");

    if (payload_len < 0 || payload_len >= preferred_size) {
        LOG_ERR("status: Error during JSON creation\n");
        coap_set_status_code(response, INTERNAL_SERVER_ERROR_5_00);
        coap_set_payload(response, buffer, 0);
        return;
    }

    for (int i = 0; i < MAX_VEHICLES; i++) {
        EV ev = vehicles[i];
        int current_status = status[i];  // current status

        int written = snprintf((char *)buffer + payload_len, preferred_size - payload_len,
                               "{\"id\":%d,\"battery\":%d,\"priority\":%d,\"status\":%d}%s",
                               ev.id, ev.battery, ev.priority, current_status,
                               (i < MAX_VEHICLES - 1) ? "," : "]");

        if (written < 0 || written >= (preferred_size - payload_len)) {
            LOG_ERR("status: buffer overflow during writing vehicles data %d\n", i);
            coap_set_status_code(response, INTERNAL_SERVER_ERROR_5_00);
            coap_set_payload(response, buffer, 0);
            return;
        }

        payload_len += written;
    }

    // close the JSON object
    if (payload_len < preferred_size - 2) {
        buffer[payload_len++] = '}';
        buffer[payload_len] = '\0';
    } else {
        LOG_ERR("status: buffer too small to close the JSON\n");
        coap_set_status_code(response, INTERNAL_SERVER_ERROR_5_00);
        coap_set_payload(response, buffer, 0);
        return;
    }

    // header and payload of the response
    coap_set_header_content_format(response, APPLICATION_JSON);
    coap_set_header_etag(response, (uint8_t *)&payload_len, 1);
    coap_set_payload(response, buffer, payload_len);

    LOG_INFO("Actuator: send status: %s (size: %d)\n", buffer, payload_len);
}

// CoAP event handler
static void res_event_handler(void) {

    LOG_INFO("Actuator: NEW-EVENT (energy sampled or button pressed) \n");
    //call the function to compute the schedule
    schedule();
    //update leds based to the schedules
    bool any_waiting = false;
    bool any_charging = false;
    bool any_vehicle = false;
    bool only_high_priority_charging = true;

    for(int i = 0; i < MAX_VEHICLES; i++) {
        if(vehicles[i].id != -1){
            any_vehicle = true;
        }
        //check if there are some EV connected but not in charge
        if (vehicles[i].id != -1 && charging_slots[i] == 0) {
            any_waiting = true; 
        }
        //check if there are EV in charge
        if (charging_slots[i] == 1) {
            any_charging = true;
            //check if only high priorty EV are in charge
            if (vehicles[i].priority == 0) {
                only_high_priority_charging = false;
            }
            
        }
    }

    // switch off the RGB LED before setting it
    leds_off(LEDS_RED + LEDS_GREEN + LEDS_BLUE);

    if(!any_vehicle){
        leds_on(LEDS_GREEN); // there are not waiting vehicles
    }else {
        // yellow LED on: almost one waiting EV
        if (any_waiting) {
            leds_single_on(LEDS_YELLOW);
        } else {
            leds_single_off(LEDS_YELLOW);
            leds_on(LEDS_GREEN); // all present EV are in charge
        }

        if (!any_charging) {
            leds_on(LEDS_RED); // no present EV in charge
        } else if (only_high_priority_charging) {
            leds_on(LEDS_BLUE); // only EV with High priorty are charge
        }
    }
}