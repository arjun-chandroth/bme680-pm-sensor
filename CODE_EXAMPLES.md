# PM Sensor Integration - Code Examples

## How the PM Sensor Integration Works

### 1. Firmware Level Integration

#### Reading PM Values (bme680_test.c)
```c
// PM sensor is read every 3 seconds in the main loop
static void read_pm_sensor(void)
{
    if (pm_sensor != NULL) {
        pm1_0 = dfrobot_gainParticleConcentration_ugm3(pm_sensor, PARTICLE_PM1_0_ATMOSPHERE);
        pm2_5 = dfrobot_gainParticleConcentration_ugm3(pm_sensor, PARTICLE_PM2_5_ATMOSPHERE);
        pm10 = dfrobot_gainParticleConcentration_ugm3(pm_sensor, PARTICLE_PM10_ATMOSPHERE);
        calculate_aqi();
    }
}
```

#### AQI Calculation (based on US EPA standard)
```c
static void calculate_aqi(void)
{
    float pm25_concentration = pm2_5;
    
    // These are the EPA standard breakpoints
    if (pm25_concentration <= 12.0) {
        aqi = pm25_concentration * (50.0 / 12.0);
        strcpy(aqi_level, "Good");
    }
    else if (pm25_concentration <= 35.4) {
        // Linear interpolation between breakpoints
        aqi = 50.0 + (pm25_concentration - 12.0) * ((100.0 - 50.0) / (35.4 - 12.0));
        strcpy(aqi_level, "Moderate");
    }
    // ... more breakpoints for higher PM concentrations
    else {
        aqi = 300.0;
        strcpy(aqi_level, "Hazardous");
    }
}
```

#### Sensor Initialization
```c
// In app_main()
pm_sensor = dfrobot_create(I2C_MASTER_NUM, PM_SENSOR_I2C_ADDR);
if (pm_sensor == NULL) {
    ESP_LOGE(TAG, "Failed to create PM sensor");
} else {
    vTaskDelay(pdMS_TO_TICKS(100));
    if (!dfrobot_begin(pm_sensor)) {
        ESP_LOGW(TAG, "PM sensor not found at address 0x%02X", PM_SENSOR_I2C_ADDR);
        dfrobot_delete(pm_sensor);
        pm_sensor = NULL;
    } else {
        uint8_t version = dfrobot_gainVersion(pm_sensor);
        ESP_LOGI(TAG, "PM Sensor initialized. Version: 0x%02X", version);
    }
}
```

#### JSON Output to Serial/API
```c
static void print_sensor_data(void)
{
    printf("{\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f,\"iaq\":%.1f,\"h2s\":%d,\"odor\":%d,\"pm1_0\":%u,\"pm2_5\":%u,\"pm10\":%u,\"aqi\":%.1f,\"aqi_level\":\"%s\"}\n",
           temperature, humidity, pressure, iaq, h2s_raw, odor_raw, pm1_0, pm2_5, pm10, aqi, aqi_level);
}
```

### 2. Driver Level (DFRobot_AirQualitySensor.c)

#### I2C Communication
```c
// Low-level I2C read function
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
```

#### Reading Particle Concentration
```c
uint16_t dfrobot_gainParticleConcentration_ugm3(DFRobot_AirQualitySensor* sensor, uint8_t type)
{
    uint8_t reg = PM2_5_ATMOS_REG;  // default to PM2.5
    
    // Select register based on PM type
    if (type == PARTICLE_PM1_0_ATMOSPHERE)
        reg = PM1_0_ATMOS_REG;      // Register 0x03
    else if (type == PARTICLE_PM2_5_ATMOSPHERE)
        reg = PM2_5_ATMOS_REG;      // Register 0x04
    else if (type == PARTICLE_PM10_ATMOSPHERE)
        reg = PM10_ATMOS_REG;       // Register 0x05
    
    uint8_t data[2] = {0, 0};
    if (i2c_read_bytes(sensor, reg, data, 2) != 0) {
        ESP_LOGE(TAG, "Failed to read PM data from register 0x%02X", reg);
        return 0;
    }
    
    // Combine two bytes into 16-bit value (Big-Endian)
    uint16_t value = (data[0] << 8) | data[1];
    return value;
}
```

### 3. Web UI Integration

#### Data Model (app.js)
```javascript
let intakeData = {
    // Existing fields...
    iaq: 50, temperature: 24.5, humidity: 45,
    // New PM fields
    pm1_0: 10,           // PM1.0 in µg/m³
    pm2_5: 25,           // PM2.5 in µg/m³
    pm10: 40,            // PM10 in µg/m³
    aqi: 60,             // Calculated AQI score (0-500+)
    aqi_level: "Moderate" // Text description
};
```

#### AQI Calculation (JavaScript - US EPA Standard)
```javascript
function calculateAQI(dataSet) {
    const pm25 = dataSet.pm2_5;
    
    // EPA Standard Breakpoints for PM2.5
    if (pm25 <= 12.0) {
        dataSet.aqi = pm25 * (50.0 / 12.0);
        dataSet.aqi_level = "Good";
    }
    else if (pm25 <= 35.4) {
        dataSet.aqi = 50.0 + (pm25 - 12.0) * ((100.0 - 50.0) / (35.4 - 12.0));
        dataSet.aqi_level = "Moderate";
    }
    else if (pm25 <= 55.4) {
        dataSet.aqi = 100.0 + (pm25 - 35.4) * ((150.0 - 100.0) / (55.4 - 35.4));
        dataSet.aqi_level = "Unhealthy for Sensitive Groups";
    }
    else if (pm25 <= 150.4) {
        dataSet.aqi = 150.0 + (pm25 - 55.4) * ((200.0 - 150.0) / (150.4 - 55.4));
        dataSet.aqi_level = "Unhealthy";
    }
    else if (pm25 <= 250.4) {
        dataSet.aqi = 200.0 + (pm25 - 150.4) * ((300.0 - 200.0) / (250.4 - 150.4));
        dataSet.aqi_level = "Very Unhealthy";
    }
    else {
        dataSet.aqi = 300.0;
        dataSet.aqi_level = "Hazardous";
    }
}
```

#### Display Update (app.js)
```javascript
function updateIntakeSensors() {
    // ... existing code ...
    
    // Update PM values
    document.getElementById('intakePm1').textContent = Math.round(intakeData.pm1_0);
    document.getElementById('intakePm25').textContent = Math.round(intakeData.pm2_5);
    document.getElementById('intakePm10').textContent = Math.round(intakeData.pm10);
    
    // Update AQI display with color coding
    document.getElementById('intakeAqiScore').textContent = Math.round(intakeData.aqi);
    const aqiLevelEl = document.getElementById('intakeAqiLevel');
    aqiLevelEl.textContent = intakeData.aqi_level;
    aqiLevelEl.className = 'value-lg aqi-level ' + getAQIClass(intakeData.aqi_level);
}

function getAQIClass(level) {
    if (level === "Good") return "aqi-good";
    if (level === "Moderate") return "aqi-moderate";
    if (level.includes("Unhealthy for Sensitive")) return "aqi-sensitive";
    if (level === "Unhealthy") return "aqi-unhealthy";
    if (level === "Very Unhealthy") return "aqi-very-unhealthy";
    if (level === "Hazardous") return "aqi-hazardous";
    return "";
}
```

#### API Data Fetch (app.js)
```javascript
function fetchSensorData() {
    const apiUrl = 'http://localhost:8888/api/sensors';
    
    fetch(apiUrl, { mode: 'cors', method: 'GET' })
        .then(response => response.json())
        .then(data => {
            intakeData = {
                // ... existing fields ...
                // New PM fields from API
                pm1_0: data.pm1_0 || intakeData.pm1_0,
                pm2_5: data.pm2_5 || intakeData.pm2_5,
                pm10: data.pm10 || intakeData.pm10,
                aqi: data.aqi || intakeData.aqi,
                aqi_level: data.aqi_level || intakeData.aqi_level
            };
            
            // Apply PM reduction for exhaust (30% of intake)
            exhaustData.pm1_0 = intakeData.pm1_0 * 0.3;
            exhaustData.pm2_5 = intakeData.pm2_5 * 0.3;
            exhaustData.pm10 = intakeData.pm10 * 0.3;
            
            // Recalculate AQI for exhaust
            calculateAQI(exhaustData);
            
            setConnectionStatus(true);
            updateAllDisplay();
        })
        .catch(error => {
            console.error('Failed to fetch sensor data:', error);
            setConnectionStatus(false);
        });
}
```

### 4. HTML Markup (index.html)

#### PM Section
```html
<!-- Particulate Matter Data -->
<section class="section pm-section">
    <h2>Particulate Matter (PM)</h2>
    <div class="sensor-grid">
        <div class="sensor-card">
            <div class="sensor-header"><h3>PM1.0</h3></div>
            <div class="sensor-body">
                <div class="sensor-value-large">
                    <span class="value-lg" id="intakePm1">--</span>
                    <span class="unit-lg">µg/m³</span>
                </div>
            </div>
        </div>
        <!-- PM2.5 and PM10 similar cards -->
    </div>
</section>
```

#### AQI Section
```html
<!-- AQI Data -->
<section class="section aqi-section">
    <h2>Air Quality Index (AQI)</h2>
    <div class="sensor-grid">
        <div class="sensor-card">
            <div class="sensor-header"><h3>AQI Score</h3></div>
            <div class="sensor-body">
                <div class="sensor-value-large">
                    <span class="value-lg" id="intakeAqiScore">--</span>
                </div>
            </div>
        </div>
        <div class="sensor-card">
            <div class="sensor-header"><h3>AQI Level</h3></div>
            <div class="sensor-body">
                <div class="sensor-value-large">
                    <span class="value-lg aqi-level" id="intakeAqiLevel">--</span>
                </div>
            </div>
        </div>
    </div>
</section>
```

### 5. CSS Styling (styles.css)

#### AQI Color Classes
```css
.aqi-level {
    padding: 0.5rem 1rem;
    border-radius: 6px;
    font-weight: bold;
    text-align: center;
    display: inline-block;
    min-width: 200px;
    margin-top: 0.5rem;
}

.aqi-good {
    background-color: #4CAF50;  /* Green */
    color: white;
}

.aqi-moderate {
    background-color: #FFC107;  /* Yellow */
    color: #212121;
}

.aqi-sensitive {
    background-color: #FF9800;  /* Orange */
    color: white;
}

.aqi-unhealthy {
    background-color: #F44336;  /* Red */
    color: white;
}

.aqi-very-unhealthy {
    background-color: #9C27B0;  /* Purple */
    color: white;
}

.aqi-hazardous {
    background-color: #5D1A1A;  /* Dark Red */
    color: white;
}
```

## Data Flow Diagram

```
┌─────────────────────────────────────────────┐
│     PM Sensor (I2C Address 0x19)            │
│  - PM1.0 (Register 0x03)                    │
│  - PM2.5 (Register 0x04)                    │
│  - PM10 (Register 0x05)                     │
└──────────────────┬──────────────────────────┘
                   │ (I2C Read)
                   ▼
┌─────────────────────────────────────────────┐
│  DFRobot_AirQualitySensor Driver            │
│  - dfrobot_gainParticleConcentration()      │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────┐
│  bme680_test.c (ESP32 Firmware)             │
│  - read_pm_sensor()                         │
│  - calculate_aqi()                          │
│  - print_sensor_data() → JSON               │
└──────────────────┬──────────────────────────┘
                   │ (Serial Output & HTTP API)
                   ▼
┌─────────────────────────────────────────────┐
│  Web UI (index.html, app.js)                │
│  - Fetch /api/sensors                       │
│  - Calculate AQI (JavaScript)               │
│  - Update DOM with PM & AQI                 │
│  - Color-code AQI level                     │
└─────────────────────────────────────────────┘
```

## Backward Compatibility

All changes are fully backward compatible:
- If PM sensor is not found, system continues with existing features
- Existing sensor data (BME680, H2S, Odor) remains unchanged
- Web UI gracefully handles missing PM data in simulated mode
- JSON output includes PM fields only if sensor is present
