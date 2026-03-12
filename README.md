# SmartCharge - EV Charging Optimization System

**SmartCharge** is an **IoT-based dynamic scheduling system** for EV charging stations powered by **renewable wind energy**. It prioritizes vehicles based on **battery level + user-defined priority (High/Low)**, adapts to real-time energy production, and uses **ML predictions** to minimize grid dependency.

## Architecture Overview

**Energy Area**: Wind turbine + Energy sensor (samples 5kWh base, ML predicts next 15min)
**Charging Area**: 5 EV slots with sensors (battery %, priority 0/1) + Actuator (scheduler, LED status)
**Communication**: WSN with CoAP protocol (decentralized, no MQTT broker)
**Backend**: Border Router + Python CoAP Server + MySQL database
**User App**: Python CLI for priority control, status monitoring, energy logs

## Core Workflow

1. **Sensor**: Wakes every 15min (configurable: 15/30/45/60) → Samples current energy → **Random Forest** predicts next period → Notifies observers (green LED)
2. **Actuator**: Receives energy update → Collects EV data → **Schedules** (High prio > Low battery) → Activates slots (1.5kWh/EV)
3. **Button Simulation**: Short press=Add EV | Long press=Remove charged/random
4. **Data Flow**: SenML (energy, ×10000 int for MCU) + JSON (status)

## ML Energy Prediction

**Dataset**: Kaggle Wind Power (2018, 15min resampled, exclude 2-7AM)
**Features**: Month/Day/HourDecimal (continuous)
**Model**: Random Forest Regressor (n_estimators=5, max_depth=12) → emlearn C array
**Performance**: MSE=0.0060 | R²=0.9226 | Binned Accuracy=79%

## Scheduler Logic

```
1. Filter EVs: battery < 100%
2. Sort: High prio DESC, battery ASC  
3. While energy ≥ 1.5kWh:
   - Charge highest prio EV
   - energy -= 1.5kWh
4. LED Status:
   - Green: All charging/empty
   - Red: All waiting
   - Yellow: Mixed
   - Blue: High prio only
```


## Data Storage (MySQL)

```sql
nodes: ip(PK), name(sensor/actuator), resource, settings
energy: timestamp(PK), predicted, sampled (Kwh)
vehicles: uid(PK), id(-1=free), battery, priority(0/1), charging(0/1)
```


## User App Commands

```
status                # Slot overview
show-nodes            # Registered nodes  
set-priority <id> <0/1>
get-priority [<id>]   # All/single
get-energy            # Last 24h logs
set-sampling-period <15|30|45|60>
exit
```


## Data Formats

**SenML (Energy)**:

```json
[{"bn":"Sensor:energy:","bu":"Kwh"},
 {"n":"predicted","t":"2025-06-02T13:14Z","v":164030000},
 {"n":"sampled","v":154510000}]
```


## Tech Stack

- **Hardware**: nRF52840 MCUs, WSN, Border Router
- **Protocol**: CoAP (decentralized, real-time)
- **Backend**: Python CoAP Server
- **ML**: TinyML (emlearn C integration)

