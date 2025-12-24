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

/* ================= I2C CONFIG ================= */
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_SDA_IO    21
#define I2C_MASTER_SCL_IO    22
#define I2C_MASTER_FREQ_HZ   400000  // Use 400 kHz Fast Mode (within 3.4 MHz spec)

#define BME68X_I2C_ADDR      BME68X_I2C_ADDR_LOW  // 0x76
/* =============================================== */

/* ================= SENSOR PINS CONFIG ================= */
#define H2S_SENSOR_PIN       34   // ADC pin for H2S sensor
#define ODOR_SENSOR_PIN      35   // ADC pin for Odor sensor (you can use any ADC-capable pin)
/* ====================================================== */

static const char *TAG = "BME680";

/* ================= GLOBAL VARIABLES ================= */
static struct bme68x_dev bme_dev;
static uint8_t bme_addr = BME68X_I2C_ADDR;

static int h2s_sensor_value = 0;
static int odor_sensor_value = 0;
static adc_oneshot_unit_handle_t adc_handle = NULL;

// Sensor data structure for HTTP response
typedef struct {
    float iaq;
    int iaq_accuracy;
    float static_iaq;
    float eco2;
    float bvoc;
    float temperature;
    float humidity;
    float pressure;
    uint32_t gas_resistance;
    int h2s_raw;
    float h2s_voltage;
    int odor_raw;
    float odor_voltage;
    float stabilization;
    float run_in;
    float comp_temp;
    float comp_hum;
} sensor_data_t;

static sensor_data_t sensor_data = {0};
static SemaphoreHandle_t sensor_data_mutex = NULL;

/* ====================================================== */
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
    vTaskDelay(pdMS_TO_TICKS((period + 999) / 1000)); // Convert µs to ms, round up
}

/* ================= ADC FUNCTIONS ================= */
static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    
    adc_oneshot_new_unit(&init_config, &adc_handle);
    
    /* Configure ADC channel for H2S sensor (GPIO 34 = ADC1_CH6) */
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,  // Full range: 0-3.3V
    };
    
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &config);  // GPIO 34
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_7, &config);  // GPIO 35
    
    ESP_LOGI(TAG, "ADC initialized for H2S and Odor sensors");
}

static void read_h2s_sensor(void)
{
    int adc_raw;
    adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &adc_raw);
    h2s_sensor_value = adc_raw;
    
    float voltage = h2s_sensor_value * (3.3 / 4095.0);
    ESP_LOGI(TAG, "H2S Sensor - Raw: %d, Voltage: %.3f V", h2s_sensor_value, voltage);
}

static void read_odor_sensor(void)
{
    int adc_raw;
    adc_oneshot_read(adc_handle, ADC_CHANNEL_7, &adc_raw);
    odor_sensor_value = adc_raw;
    
    float voltage = odor_sensor_value * (3.3 / 4095.0);
    ESP_LOGI(TAG, "Odor Sensor - Raw: %d, Voltage: %.3f V", odor_sensor_value, voltage);
}
/* ================================================== */

/* ================= SERIAL JSON OUTPUT ================= */
static void send_json_to_serial(void)
{
    if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(100))) {
        printf("{\"iaq\":%.1f,\"iaq_accuracy\":%d,\"static_iaq\":%.1f,\"eco2\":%.1f,"
            "\"bvoc\":%.2f,\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f,"
            "\"gas_resistance\":%lu,\"h2s_raw\":%d,\"h2s_voltage\":%.3f,"
            "\"odor_raw\":%d,\"odor_voltage\":%.3f,\"stabilization\":%.1f,"
            "\"run_in\":%.1f,\"comp_temp\":%.2f,\"comp_hum\":%.2f}\n",
            sensor_data.iaq, sensor_data.iaq_accuracy, sensor_data.static_iaq,
            sensor_data.eco2, sensor_data.bvoc, sensor_data.temperature,
            sensor_data.humidity, sensor_data.pressure, sensor_data.gas_resistance,
            sensor_data.h2s_raw, sensor_data.h2s_voltage,
            sensor_data.odor_raw, sensor_data.odor_voltage,
            sensor_data.stabilization, sensor_data.run_in,
            sensor_data.comp_temp, sensor_data.comp_hum);
        xSemaphoreGive(sensor_data_mutex);
    }
}

/* ============================================================ */
static void check_bsec_status(bsec_library_return_t bsec_status, const char *function_name)
{
    if (bsec_status != BSEC_OK) {
        ESP_LOGE(TAG, "%s failed with status: %d", function_name, bsec_status);
    }
}

/* ================= MAIN APP ================= */
void app_main(void)
{
    ESP_LOGI(TAG, "Starting BME680 with BSEC2...");
    
    // Create sensor data mutex
    sensor_data_mutex = xSemaphoreCreateMutex();
    
    /* -------- I2C INIT -------- */
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
    
    ESP_LOGI(TAG, "I2C initialized");
    
    /* -------- ADC INIT -------- */
    adc_init();
    
    ESP_LOGI(TAG, "ADC initialized for gas sensors");
    
    /* -------- BME680 INIT -------- */
    memset(&bme_dev, 0, sizeof(bme_dev));
    
    bme_dev.intf = BME68X_I2C_INTF;
    bme_dev.intf_ptr = &bme_addr;
    bme_dev.read = i2c_read;
    bme_dev.write = i2c_write;
    bme_dev.delay_us = delay_us;
    bme_dev.amb_temp = 25;
    
    bsec_library_return_t bsec_status = bme68x_init(&bme_dev);
    if (bsec_status != BME68X_OK) {
        ESP_LOGE(TAG, "BME680 init failed: %d", bsec_status);
        return;
    }
    
    ESP_LOGI(TAG, "BME680 initialized");
    ESP_LOGI(TAG, "Chip ID: 0x%02X", bme_dev.chip_id);
    
    /* -------- SENSOR CONFIG -------- */
    struct bme68x_conf conf_sensor;
    conf_sensor.os_hum  = BME68X_OS_2X;
    conf_sensor.os_pres = BME68X_OS_4X;
    conf_sensor.os_temp = BME68X_OS_8X;
    conf_sensor.filter  = BME68X_FILTER_SIZE_3;
    
    // Set configuration BEFORE heater settings (per Section 3.2.2)
    bme68x_set_conf(&conf_sensor, &bme_dev);
    
    struct bme68x_heatr_conf heatr_conf;
    heatr_conf.enable = BME68X_ENABLE;
    heatr_conf.heatr_temp = 320;  // Heater temperature in °C
    heatr_conf.heatr_dur = 150;   // Heater duration in ms
    
    bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, &bme_dev);
    
    /* -------- BSEC INIT -------- */
    bsec_version_t version;
    bsec_status = bsec_get_version(&version);
    check_bsec_status(bsec_status, "bsec_get_version");
    
    ESP_LOGI(TAG, "BSEC Version: %d.%d.%d.%d", 
             version.major, version.minor, version.major_bugfix, version.minor_bugfix);
    
    /* Initialize BSEC library FIRST */
    bsec_status = bsec_init();
    check_bsec_status(bsec_status, "bsec_init");
    
    if (bsec_status != BSEC_OK) {
        ESP_LOGE(TAG, "BSEC init failed!");
        return;
    }
    
    /* Configure BSEC with simple output subscription */
    bsec_sensor_configuration_t requested_virtual_sensors[13];
    uint8_t n_requested_virtual_sensors = 0;
    
    /* Only request essential outputs to avoid compatibility issues */
    bsec_virtual_sensor_t sensor_list[] = {
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_STATIC_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_GAS,
        BSEC_OUTPUT_STABILIZATION_STATUS,
        BSEC_OUTPUT_RUN_IN_STATUS
    };
    
    uint8_t n_sensors = sizeof(sensor_list) / sizeof(sensor_list[0]);
    for (uint8_t i = 0; i < n_sensors; i++) {
        requested_virtual_sensors[i].sensor_id = sensor_list[i];
        requested_virtual_sensors[i].sample_rate = BSEC_SAMPLE_RATE_LP; // Low power rate 3.33 Hz
    }
    n_requested_virtual_sensors = n_sensors;
    
    bsec_sensor_configuration_t required_sensor_settings[BSEC_MAX_PHYSICAL_SENSOR];
    uint8_t n_required_sensor_settings = BSEC_MAX_PHYSICAL_SENSOR;
    
    bsec_status = bsec_update_subscription(requested_virtual_sensors, 
                                          n_requested_virtual_sensors,
                                          required_sensor_settings, 
                                          &n_required_sensor_settings);
    
    if (bsec_status != BSEC_OK) {
        ESP_LOGW(TAG, "bsec_update_subscription warning status: %d", bsec_status);
    }
    
    // Log required sensor settings
    ESP_LOGI(TAG, "BSEC requires %d physical sensor settings", n_required_sensor_settings);
    for (uint8_t i = 0; i < n_required_sensor_settings; i++) {
        ESP_LOGI(TAG, "  Sensor %d: ID=%d, Rate=%.4f", i,
                 required_sensor_settings[i].sensor_id,
                 required_sensor_settings[i].sample_rate);
    }
    
    /* -------- MAIN LOOP -------- */
    ESP_LOGI(TAG, "Entering main loop - JSON output to serial...");
    
    while (1) {
        struct bme68x_data data;
        uint8_t n_fields = 0;
        
        /* Trigger measurement */
        bme68x_set_op_mode(BME68X_FORCED_MODE, &bme_dev);
        
        /* Wait for measurement to complete */
        uint32_t meas_dur = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf_sensor, &bme_dev);
        // meas_dur is in microseconds; add 10ms extra margin for safety
        vTaskDelay(pdMS_TO_TICKS((meas_dur / 1000) + 10));
        
        /* Read data */
        bsec_status = bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &bme_dev);
        if (bsec_status != BME68X_OK || n_fields == 0) {
            ESP_LOGW(TAG, "No data available, retrying...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        bool gas_valid = (data.status & BME68X_GASM_VALID_MSK) && 
                         (data.status & BME68X_HEAT_STAB_MSK);
        
        ESP_LOGI(TAG, "RAW: T=%.2f°C H=%.2f%% P=%.2fhPa G=%luΩ %s",
                 data.temperature,
                 data.humidity,
                 data.pressure / 100.0f,
                 data.gas_resistance,
                 gas_valid ? "(valid)" : "(invalid)");
        
        /* Prepare BSEC inputs */
        bsec_input_t inputs[4]; // Temperature, Humidity, Pressure, Gas
        uint8_t n_inputs = 0;
        
        int64_t timestamp_ns = esp_timer_get_time() * 1000LL; // Convert µs to ns
        
        /* Temperature */
        inputs[n_inputs].sensor_id = BSEC_INPUT_TEMPERATURE;
        inputs[n_inputs].signal = data.temperature;
        inputs[n_inputs].time_stamp = timestamp_ns;
        n_inputs++;
        
        /* Humidity */
        inputs[n_inputs].sensor_id = BSEC_INPUT_HUMIDITY;
        inputs[n_inputs].signal = data.humidity;
        inputs[n_inputs].time_stamp = timestamp_ns;
        n_inputs++;
        
        /* Pressure (convert hPa to Pa) */
        inputs[n_inputs].sensor_id = BSEC_INPUT_PRESSURE;
        inputs[n_inputs].signal = data.pressure * 100.0f;
        inputs[n_inputs].time_stamp = timestamp_ns;
        n_inputs++;
        
        /* Gas resistance (if valid) */
        if (gas_valid) {
            inputs[n_inputs].sensor_id = BSEC_INPUT_GASRESISTOR;
            inputs[n_inputs].signal = (float)data.gas_resistance;
            inputs[n_inputs].time_stamp = timestamp_ns;
            n_inputs++;
        }
        
        /* Process BSEC outputs */
        bsec_output_t outputs[BSEC_NUMBER_OUTPUTS];
        uint8_t n_outputs = BSEC_NUMBER_OUTPUTS;
        
        bsec_status = bsec_do_steps(inputs, n_inputs, outputs, &n_outputs);
        
        if (bsec_status != BSEC_OK) {
            ESP_LOGW(TAG, "BSEC step warning: %d", bsec_status);
        }
        
        if (n_outputs > 0) {
            for (int i = 0; i < n_outputs; i++) {
                switch (outputs[i].sensor_id) {
                    case BSEC_OUTPUT_IAQ:
                        ESP_LOGI(TAG, "IAQ: %.0f (Accuracy: %d)", 
                                 outputs[i].signal, outputs[i].accuracy);
                        if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(100))) {
                            sensor_data.iaq = outputs[i].signal;
                            sensor_data.iaq_accuracy = outputs[i].accuracy;
                            xSemaphoreGive(sensor_data_mutex);
                        }
                        break;
                    case BSEC_OUTPUT_STATIC_IAQ:
                        ESP_LOGI(TAG, "Static IAQ: %.0f (Accuracy: %d)", 
                                 outputs[i].signal, outputs[i].accuracy);
                        if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(100))) {
                            sensor_data.static_iaq = outputs[i].signal;
                            xSemaphoreGive(sensor_data_mutex);
                        }
                        break;
                    case BSEC_OUTPUT_CO2_EQUIVALENT:
                        ESP_LOGI(TAG, "eCO2: %.0f ppm", outputs[i].signal);
                        if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(100))) {
                            sensor_data.eco2 = outputs[i].signal;
                            xSemaphoreGive(sensor_data_mutex);
                        }
                        break;
                    case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
                        ESP_LOGI(TAG, "bVOC: %.3f ppm", outputs[i].signal);
                        if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(100))) {
                            sensor_data.bvoc = outputs[i].signal;
                            xSemaphoreGive(sensor_data_mutex);
                        }
                        break;
                    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
                        ESP_LOGI(TAG, "Comp Temp: %.2f°C", outputs[i].signal);
                        if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(100))) {
                            sensor_data.comp_temp = outputs[i].signal;
                            xSemaphoreGive(sensor_data_mutex);
                        }
                        break;
                    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
                        ESP_LOGI(TAG, "Comp Hum: %.2f%%", outputs[i].signal);
                        if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(100))) {
                            sensor_data.comp_hum = outputs[i].signal;
                            xSemaphoreGive(sensor_data_mutex);
                        }
                        break;
                    case BSEC_OUTPUT_RAW_GAS:
                        ESP_LOGI(TAG, "Comp Gas: %.0fΩ", outputs[i].signal);
                        if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(100))) {
                            sensor_data.gas_resistance = (uint32_t)outputs[i].signal;
                            xSemaphoreGive(sensor_data_mutex);
                        }
                        break;
                    case BSEC_OUTPUT_STABILIZATION_STATUS:
                        ESP_LOGI(TAG, "Stabilization: %.0f%%", outputs[i].signal);
                        if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(100))) {
                            sensor_data.stabilization = outputs[i].signal;
                            xSemaphoreGive(sensor_data_mutex);
                        }
                        break;
                    case BSEC_OUTPUT_RUN_IN_STATUS:
                        ESP_LOGI(TAG, "Run-in: %.0f%%", outputs[i].signal);
                        if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(100))) {
                            sensor_data.run_in = outputs[i].signal;
                            xSemaphoreGive(sensor_data_mutex);
                        }
                        break;
                    default:
                        ESP_LOGD(TAG, "Output ID %d: %.3f", 
                                outputs[i].sensor_id, outputs[i].signal);
                        break;
                }
            }
            ESP_LOGI(TAG, "---");
        } else if (bsec_status != BSEC_OK) {
            ESP_LOGE(TAG, "BSEC step failed: %d", bsec_status);
        }
        
        /* -------- READ GAS SENSORS -------- */
        read_h2s_sensor();
        read_odor_sensor();
        
        // Store gas sensor data
        if (xSemaphoreTake(sensor_data_mutex, pdMS_TO_TICKS(100))) {
            sensor_data.h2s_raw = h2s_sensor_value;
            sensor_data.h2s_voltage = h2s_sensor_value * (3.3 / 4095.0);
            sensor_data.odor_raw = odor_sensor_value;
            sensor_data.odor_voltage = odor_sensor_value * (3.3 / 4095.0);
            sensor_data.temperature = data.temperature;
            sensor_data.humidity = data.humidity;
            sensor_data.pressure = data.pressure / 100.0f;
            xSemaphoreGive(sensor_data_mutex);
        }
        
        // Output JSON to serial
        send_json_to_serial();
        
        /* Wait for next measurement */
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
