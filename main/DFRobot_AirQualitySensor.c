#include "DFRobot_AirQualitySensor.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "DFRobot_AQS";

#define SENSOR_VERSION_REG      0x00
#define PM1_0_ATMOS_REG         0x03
#define PM2_5_ATMOS_REG         0x04
#define PM10_ATMOS_REG          0x05

static int8_t i2c_read_bytes(DFRobot_AirQualitySensor* sensor, uint8_t reg, uint8_t* data, uint32_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sensor->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (sensor->i2c_addr << 1) | I2C_MASTER_READ, true);
    
    if (len > 1)
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(sensor->i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK) ? 0 : -1;
}

DFRobot_AirQualitySensor* dfrobot_create(i2c_port_t port, uint8_t addr)
{
    DFRobot_AirQualitySensor* sensor = (DFRobot_AirQualitySensor*)malloc(sizeof(DFRobot_AirQualitySensor));
    if (sensor == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for sensor");
        return NULL;
    }
    
    sensor->i2c_port = port;
    sensor->i2c_addr = addr;
    
    return sensor;
}

int dfrobot_begin(DFRobot_AirQualitySensor* sensor)
{
    uint8_t version = 0;
    if (i2c_read_bytes(sensor, SENSOR_VERSION_REG, &version, 1) != 0) {
        ESP_LOGE(TAG, "Sensor not found at address 0x%02X", sensor->i2c_addr);
        return 0;
    }
    
    ESP_LOGI(TAG, "Sensor found! Version: 0x%02X", version);
    return 1;
}

uint16_t dfrobot_gainParticleConcentration_ugm3(DFRobot_AirQualitySensor* sensor, uint8_t type)
{
    uint8_t reg = PM2_5_ATMOS_REG;  // default to PM2.5
    
    if (type == PARTICLE_PM1_0_ATMOSPHERE)
        reg = PM1_0_ATMOS_REG;
    else if (type == PARTICLE_PM2_5_ATMOSPHERE)
        reg = PM2_5_ATMOS_REG;
    else if (type == PARTICLE_PM10_ATMOSPHERE)
        reg = PM10_ATMOS_REG;
    
    uint8_t data[2] = {0, 0};
    if (i2c_read_bytes(sensor, reg, data, 2) != 0) {
        ESP_LOGE(TAG, "Failed to read PM data from register 0x%02X", reg);
        return 0;
    }
    
    uint16_t value = (data[0] << 8) | data[1];
    return value;
}

uint8_t dfrobot_gainVersion(DFRobot_AirQualitySensor* sensor)
{
    uint8_t version = 0;
    i2c_read_bytes(sensor, SENSOR_VERSION_REG, &version, 1);
    return version;
}

void dfrobot_delete(DFRobot_AirQualitySensor* sensor)
{
    if (sensor != NULL) {
        free(sensor);
    }
}
