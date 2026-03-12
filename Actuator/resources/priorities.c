#include "contiki.h"
#include "coap-engine.h"
#include "sys/log.h"
#include <stdio.h>
//Utility functions
#include "../Util/encoding.h"
#include "../Util/timestamp.h"
#include "../Util/vehicles.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

//vehicles priorities
int priorities[MAX_VEHICLES] = {-1, -1, -1, -1, -1};

static void res_get_handler(coap_message_t *request, coap_message_t *response,
                          uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_put_handler(coap_message_t *request, coap_message_t *response,
                          uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

//CoAP “settings” resource definition
RESOURCE(
        res_settings, 
        "title=\"res_settings\";rt=\"json\";if=\"core.p\"", // Parametrizable resource, not observable because it does not change, no notificable
        res_get_handler, 
        NULL,
        res_put_handler,
        NULL);

// This function is called when the resource is requested
static void res_get_handler(coap_message_t *request, coap_message_t *response,
                            uint8_t *buffer, uint16_t preferred_size, int32_t *offset) {

    size_t len;
    const char *query = NULL;
    int id;

    LOG_INFO("priorities: Priority requested: \n");
    if ((len = coap_get_query_variable(request, "id", &query))) {
        id = atoi(query);

        if (id >= 0 && id < MAX_VEHICLES) {
            LOG_INFO("priorities: Only priority %d requested: \n", id);
            snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"id\":%d,\"priority\":%d}", id, priorities[id]);
            coap_set_payload(response, buffer, strlen((char *)buffer));
            coap_set_status_code(response, CONTENT_2_05);
        } else {
            snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"error\":\"Invalid ID\"}");
            coap_set_payload(response, buffer, strlen((char *)buffer));
            coap_set_status_code(response, BAD_REQUEST_4_00);
        }
    } else {
        // no id parameter: return all priorities
        LOG_INFO("priorities: ALl priorities requested \n");
        char *ptr = (char *)buffer;
        int used = snprintf(ptr, COAP_MAX_CHUNK_SIZE, "{\"priorities\":[");
        ptr += used;

        for (int i = 0; i < MAX_VEHICLES; i++) {
            used = snprintf(ptr, COAP_MAX_CHUNK_SIZE - (ptr - (char *)buffer),
                            "%d%s", priorities[i], (i < MAX_VEHICLES - 1) ? "," : "");
            ptr += used;
        }

        snprintf(ptr, COAP_MAX_CHUNK_SIZE - (ptr - (char *)buffer), "]}");

        coap_set_payload(response, buffer, strlen((char *)buffer));
        coap_set_status_code(response, CONTENT_2_05);
    }
}

// This function is called when a PUT request is received
static void res_put_handler(coap_message_t *request, coap_message_t *response,
                          uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
    LOG_INFO("Actuator:--------------NEW-PUT-SETTING-REQUEST---------------------\n");

    const char *id_str;
    const char *prio_str;
    int id, new_priority;

    if(coap_get_query_variable(request, "id", &id_str) &&
       coap_get_query_variable(request, "priority", &prio_str)) {

        id = atoi(id_str);
        new_priority = atoi(prio_str);

        if(id >= 0 && id < MAX_VEHICLES && (new_priority == 0 || new_priority == 1)) {
            priorities[id] = new_priority;
            coap_set_status_code(response, CHANGED_2_04);
            LOG_INFO("priorities: New settings: \n");
            for(int i=0; i < MAX_VEHICLES; i++){
                LOG_INFO("priorities[%i]: %i \n", i, priorities[i]);
            }
        } else {
            snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"error\":\"Invalid ID\"}");
            coap_set_payload(response, buffer, strlen((char *)buffer));
            coap_set_status_code(response, BAD_REQUEST_4_00);
            LOG_INFO("priorities: invalid id \n");
        }
    } else {
        coap_set_status_code(response, BAD_REQUEST_4_00);
        LOG_INFO("priorities: error \n");
    }
}