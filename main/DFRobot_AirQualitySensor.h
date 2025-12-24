#ifndef DFROBOT_AirQualitySensor_H
#define DFROBOT_AirQualitySensor_H

#include <stdint.h>
#include "driver/i2c.h"

#define PARTICLE_PM1_0_ATMOSPHERE 3
#define PARTICLE_PM2_5_ATMOSPHERE 4
#define PARTICLE_PM10_ATMOSPHERE  5

typedef struct {
    i2c_port_t i2c_port;
    uint8_t i2c_addr;
} DFRobot_AirQualitySensor;

// Function declarations
DFRobot_AirQualitySensor* dfrobot_create(i2c_port_t port, uint8_t addr);
int dfrobot_begin(DFRobot_AirQualitySensor* sensor);
uint16_t dfrobot_gainParticleConcentration_ugm3(DFRobot_AirQualitySensor* sensor, uint8_t type);
uint8_t dfrobot_gainVersion(DFRobot_AirQualitySensor* sensor);
void dfrobot_delete(DFRobot_AirQualitySensor* sensor);

#endif
