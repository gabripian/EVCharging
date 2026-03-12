#ifndef VEHICLES_H
#define VEHICLES_H

#define MAX_VEHICLES 5

typedef struct {
    int id;              // id of the EV (-1 if the slot is free)
    int battery;         // battery status
    int priority;        // 1 "High" or 0 "Low"
} EV;

#endif // VEHICLES_H
