#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"

#include "bme68x.h"
#include "bme68x_defs.h"

#include "bsec_interface.h"
#include "bsec_datatypes.h"

#include "DFRobot_AirQualitySensor.h"

/* I2C CONFIG */
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_SDA_IO    21
#define I2C_MASTER_SCL_IO    22
#define I2C_MASTER_FREQ_HZ   400000

#define BME68X_I2C_ADDR      BME68X_I2C_ADDR_LOW  // 0x76
#define PM_SENSOR_I2C_ADDR   0x19

/* SENSOR PINS */
#define H2S_SENSOR_PIN       34
#define ODOR_SENSOR_PIN      35

static const char *TAG = "AIR_QUALITY";

/* GLOBAL STATE */
static struct bme68x_dev bme_dev;
static uint8_t bme_addr = BME68X_I2C_ADDR;
static adc_oneshot_unit_handle_t adc_handle = NULL;
static DFRobot_AirQualitySensor* pm_sensor = NULL;

/* SENSOR VALUES */
static float temperature = 0;
static float humidity = 0;
static float pressure = 0;
static float iaq = 0;
static int h2s_raw = 0;
static int odor_raw = 0;
static uint16_t pm1_0 = 0;
static uint16_t pm2_5 = 0;
static uint16_t pm10 = 0;
static float aqi = 0;
static char aqi_level[32] = "Unknown";

/* ===== I2C FUNCTIONS ===== */
static int8_t i2c_read(uint8_t reg, uint8_t *data, uint32_t len, void *intf_ptr)
{
    uint8_t dev_addr = *(uint8_t *)intf_ptr;
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);
    
    if (len > 1)
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

static int8_t i2c_write(uint8_t reg, const uint8_t *data, uint32_t len, void *intf_ptr)
{
    uint8_t dev_addr = *(uint8_t *)intf_ptr;
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write(cmd, (uint8_t *)data, len, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

static void delay_us(uint32_t period, void *intf_ptr)
{
    vTaskDelay(pdMS_TO_TICKS(period / 1000));
}

/* ===== ADC SETUP ===== */
static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_7, &config));
}

/* ===== AQI CALCULATION ===== */
static void calculate_aqi(void)
{
    // AQI calculation based on PM2.5 (US EPA standard)
    // This uses the breakpoint concentrations
    float pm25_concentration = pm2_5;
    
    if (pm25_concentration <= 12.0) {
        aqi = pm25_concentration * (50.0 / 12.0);
        strcpy(aqi_level, "Good");
    }
    else if (pm25_concentration <= 35.4) {
        aqi = 50.0 + (pm25_concentration - 12.0) * ((100.0 - 50.0) / (35.4 - 12.0));
        strcpy(aqi_level, "Moderate");
    }
    else if (pm25_concentration <= 55.4) {
        aqi = 100.0 + (pm25_concentration - 35.4) * ((150.0 - 100.0) / (55.4 - 35.4));
        strcpy(aqi_level, "Unhealthy for Sensitive Groups");
    }
    else if (pm25_concentration <= 150.4) {
        aqi = 150.0 + (pm25_concentration - 55.4) * ((200.0 - 150.0) / (150.4 - 55.4));
        strcpy(aqi_level, "Unhealthy");
    }
    else if (pm25_concentration <= 250.4) {
        aqi = 200.0 + (pm25_concentration - 150.4) * ((300.0 - 200.0) / (250.4 - 150.4));
        strcpy(aqi_level, "Very Unhealthy");
    }
    else {
        aqi = 300.0;
        strcpy(aqi_level, "Hazardous");
    }
}

static void read_pm_sensor(void)
{
    if (pm_sensor != NULL) {
        pm1_0 = dfrobot_gainParticleConcentration_ugm3(pm_sensor, PARTICLE_PM1_0_ATMOSPHERE);
        pm2_5 = dfrobot_gainParticleConcentration_ugm3(pm_sensor, PARTICLE_PM2_5_ATMOSPHERE);
        pm10 = dfrobot_gainParticleConcentration_ugm3(pm_sensor, PARTICLE_PM10_ATMOSPHERE);
        calculate_aqi();
    }
}

static void read_h2s(void)
{
    int adc_raw;
    adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &adc_raw);
    h2s_raw = adc_raw;
}

static void read_odor(void)
{
    int adc_raw;
    adc_oneshot_read(adc_handle, ADC_CHANNEL_7, &adc_raw);
    odor_raw = adc_raw;
}

/* ===== OUTPUT SENSOR DATA ===== */
static void print_sensor_data(void)
{
    printf("{\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f,\"iaq\":%.1f,\"h2s\":%d,\"odor\":%d,\"pm1_0\":%u,\"pm2_5\":%u,\"pm10\":%u,\"aqi\":%.1f,\"aqi_level\":\"%s\"}\n",
           temperature, humidity, pressure, iaq, h2s_raw, odor_raw, pm1_0, pm2_5, pm10, aqi, aqi_level);
}

/* ===== MAIN TASK ===== */
void app_main(void)
{
    ESP_LOGI(TAG, "Starting Air Quality Monitor");
    
    /* ===== I2C INIT ===== */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
    
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
    
    /* ===== ADC INIT ===== */
    adc_init();
    
    /* ===== PM SENSOR INIT ===== */
    pm_sensor = dfrobot_create(I2C_MASTER_NUM, PM_SENSOR_I2C_ADDR);
    if (pm_sensor == NULL) {
        ESP_LOGE(TAG, "Failed to create PM sensor");
    } else {
        vTaskDelay(pdMS_TO_TICKS(100));  // Wait for sensor to be ready
        if (!dfrobot_begin(pm_sensor)) {
            ESP_LOGW(TAG, "PM sensor not found at address 0x%02X", PM_SENSOR_I2C_ADDR);
            dfrobot_delete(pm_sensor);
            pm_sensor = NULL;
        } else {
            uint8_t version = dfrobot_gainVersion(pm_sensor);
            ESP_LOGI(TAG, "PM Sensor initialized. Version: 0x%02X", version);
        }
    }
    
    /* ===== BME680 INIT ===== */
    memset(&bme_dev, 0, sizeof(bme_dev));
    
    bme_dev.intf = BME68X_I2C_INTF;
    bme_dev.intf_ptr = &bme_addr;
    bme_dev.read = i2c_read;
    bme_dev.write = i2c_write;
    bme_dev.delay_us = delay_us;
    bme_dev.amb_temp = 25;
    
    int8_t status = bme68x_init(&bme_dev);
    if (status != BME68X_OK) {
        ESP_LOGE(TAG, "BME680 init failed: %d", status);
        return;
    }
    
    ESP_LOGI(TAG, "BME680 initialized. Chip ID: 0x%02X", bme_dev.chip_id);
    
    /* ===== SENSOR CONFIG ===== */
    struct bme68x_conf conf_sensor;
    conf_sensor.os_hum  = BME68X_OS_2X;
    conf_sensor.os_pres = BME68X_OS_4X;
    conf_sensor.os_temp = BME68X_OS_8X;
    conf_sensor.filter  = BME68X_FILTER_SIZE_3;
    
    bme68x_set_conf(&conf_sensor, &bme_dev);
    
    struct bme68x_heatr_conf heatr_conf;
    heatr_conf.enable = BME68X_ENABLE;
    heatr_conf.heatr_temp = 320;
    heatr_conf.heatr_dur = 150;
    
    bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, &bme_dev);
    
    /* ===== BSEC INIT ===== */
    bsec_library_return_t bsec_status = bsec_init();
    if (bsec_status != BSEC_OK) {
        ESP_LOGE(TAG, "BSEC init failed!");
        return;
    }
    
    bsec_sensor_configuration_t virtual_sensors[10];
    bsec_virtual_sensor_t sensor_list[] = {
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
        BSEC_OUTPUT_RAW_PRESSURE,
    };
    
    uint8_t n_sensors = sizeof(sensor_list) / sizeof(sensor_list[0]);
    for (uint8_t i = 0; i < n_sensors; i++) {
        virtual_sensors[i].sensor_id = sensor_list[i];
        virtual_sensors[i].sample_rate = BSEC_SAMPLE_RATE_LP;
    }
    
    bsec_sensor_configuration_t required_settings[BSEC_MAX_PHYSICAL_SENSOR];
    uint8_t n_required = BSEC_MAX_PHYSICAL_SENSOR;
    
    bsec_update_subscription(virtual_sensors, n_sensors, required_settings, &n_required);
    
    ESP_LOGI(TAG, "Entering measurement loop...");
    
    /* ===== MAIN LOOP ===== */
    while (1) {
        struct bme68x_data data;
        uint8_t n_fields = 0;
        
        bme68x_set_op_mode(BME68X_FORCED_MODE, &bme_dev);
        
        uint32_t meas_dur = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf_sensor, &bme_dev);
        vTaskDelay(pdMS_TO_TICKS((meas_dur / 1000) + 10));
        
        bsec_status = bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &bme_dev);
        if (bsec_status != BME68X_OK || n_fields == 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        /* BSEC Processing */
        bsec_input_t inputs[4];
        uint8_t n_inputs = 0;
        
        int64_t timestamp_ns = esp_timer_get_time() * 1000LL;
        
        inputs[n_inputs].sensor_id = BSEC_INPUT_TEMPERATURE;
        inputs[n_inputs].signal = data.temperature;
        inputs[n_inputs].time_stamp = timestamp_ns;
        n_inputs++;
        
        inputs[n_inputs].sensor_id = BSEC_INPUT_HUMIDITY;
        inputs[n_inputs].signal = data.humidity;
        inputs[n_inputs].time_stamp = timestamp_ns;
        n_inputs++;
        
        inputs[n_inputs].sensor_id = BSEC_INPUT_PRESSURE;
        inputs[n_inputs].signal = data.pressure * 100.0f;
        inputs[n_inputs].time_stamp = timestamp_ns;
        n_inputs++;
        
        bool gas_valid = (data.status & BME68X_GASM_VALID_MSK) && (data.status & BME68X_HEAT_STAB_MSK);
        if (gas_valid) {
            inputs[n_inputs].sensor_id = BSEC_INPUT_GASRESISTOR;
            inputs[n_inputs].signal = (float)data.gas_resistance;
            inputs[n_inputs].time_stamp = timestamp_ns;
            n_inputs++;
        }
        
        bsec_output_t outputs[BSEC_NUMBER_OUTPUTS];
        uint8_t n_outputs = BSEC_NUMBER_OUTPUTS;
        
        bsec_do_steps(inputs, n_inputs, outputs, &n_outputs);
        
        /* Extract IAQ from BSEC outputs */
        for (int i = 0; i < n_outputs; i++) {
            if (outputs[i].sensor_id == BSEC_OUTPUT_IAQ) {
                iaq = outputs[i].signal;
            }
        }
        
        /* Store basic BME680 readings */
        temperature = data.temperature;
        humidity = data.humidity;
        pressure = data.pressure / 100.0f;
        
        /* Read PM sensor */
        read_pm_sensor();
        
        /* Read gas sensors */
        read_h2s();
        read_odor();
        
        /* Output JSON */
        print_sensor_data();
        
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
