# PM2.5 / PM1.0 / PM10 Air Quality Sensor Integration - Summary

## Overview
Successfully integrated the DFRobot PM2.5/PM1.0/PM10 air quality sensor into the existing ESP32 air quality monitoring system. The integration includes:

- **PM Sensor Driver**: DFRobot_AirQualitySensor library (header + source)
- **Main Firmware**: Enhanced bme680_test.c with PM sensor reading and AQI calculation
- **Web UI**: Updated dashboard to display PM values and calculated AQI with descriptive levels
- **AQI Calculation**: US EPA standard PM2.5-based AQI with 6 severity levels

## Files Added/Modified

### New Files
1. **[main/DFRobot_AirQualitySensor.h](main/DFRobot_AirQualitySensor.h)**
   - Header file for PM sensor driver
   - Defines I2C communication functions
   - Declares functions for reading PM1.0, PM2.5, PM10 values

2. **[main/DFRobot_AirQualitySensor.c](main/DFRobot_AirQualitySensor.c)**
   - Implementation of PM sensor driver
   - I2C read/write functions using ESP32 driver
   - Functions to retrieve particle concentration values in µg/m³

### Modified Files
1. **[main/bme680_test.c](main/bme680_test.c)**
   - Added PM sensor initialization (I2C address 0x19)
   - Added AQI calculation function based on PM2.5 values
   - Added PM sensor reading function
   - Updated print_sensor_data() to include PM and AQI values
   - PM sensor readings integrated into main sensor loop (every 3 seconds)

2. **[main/CMakeLists.txt](main/CMakeLists.txt)**
   - Added DFRobot_AirQualitySensor.c to SRCS

3. **[web-ui/index.html](web-ui/index.html)**
   - Added "Particulate Matter (PM)" section with PM1.0, PM2.5, PM10 cards
   - Added "Air Quality Index (AQI)" section with AQI Score and AQI Level display
   - Added for both Intake and Exhaust tabs

4. **[web-ui/app.js](web-ui/app.js)**
   - Added PM data properties (pm1_0, pm2_5, pm10, aqi, aqi_level) to intakeData and exhaustData
   - Added calculateAQI() function (US EPA standard)
   - Added getAQIClass() function for styling
   - Updated simulateData() to generate realistic PM values
   - Updated updateIntakeSensors() and updateExhaustSensors() to display PM and AQI
   - Updated fetchSensorData() to parse PM and AQI from API

5. **[web-ui/styles.css](web-ui/styles.css)**
   - Added AQI level styling classes with color coding:
     - Good: Green (#4CAF50)
     - Moderate: Yellow (#FFC107)
     - Unhealthy for Sensitive Groups: Orange (#FF9800)
     - Unhealthy: Red (#F44336)
     - Very Unhealthy: Purple (#9C27B0)
     - Hazardous: Dark Red (#5D1A1A)

## Hardware Configuration

### PM Sensor
- **I2C Address**: 0x19
- **I2C Pins**: SDA=21, SCL=22
- **Register Map**:
  - 0x00: Version
  - 0x03: PM1.0 (Atmospheric)
  - 0x04: PM2.5 (Atmospheric)
  - 0x05: PM10 (Atmospheric)

## AQI Calculation (US EPA Standard)

The AQI is calculated based on PM2.5 concentration with the following breakpoints:

```
PM2.5 Range (µg/m³)  |  AQI Range  |  Level
0-12.0              |  0-50       |  Good
12.1-35.4           |  51-100     |  Moderate
35.5-55.4           |  101-150    |  Unhealthy for Sensitive Groups
55.5-150.4          |  151-200    |  Unhealthy
150.5-250.4         |  201-300    |  Very Unhealthy
250.5+              |  301+       |  Hazardous
```

## API Response Format

The sensor data is output as JSON (both to serial and HTTP API):

```json
{
  "temperature": 24.5,
  "humidity": 45.0,
  "pressure": 1013.25,
  "iaq": 50.0,
  "h2s": 100,
  "odor": 200,
  "pm1_0": 10,
  "pm2_5": 25,
  "pm10": 40,
  "aqi": 60.5,
  "aqi_level": "Moderate"
}
```

## Web Dashboard Features

### Particulate Matter Section
- Displays real-time PM1.0, PM2.5, and PM10 measurements in µg/m³
- Shows readings for both intake and exhaust air paths
- Exhaust values are calculated as 30% of intake (simulating air purification)

### AQI Section
- Shows numerical AQI score (0-500+)
- Displays descriptive air quality level with color-coded background
- Color coding indicates severity at a glance:
  - Green for good air quality
  - Red for poor air quality
  - Purple for very unhealthy conditions
  - Dark red for hazardous conditions

## Testing

### Simulated Mode
When not connected to ESP32:
- PM values are simulated with realistic variations
- AQI is automatically calculated from simulated PM2.5
- Updates every 3 seconds

### Real Hardware Mode
When connected to ESP32:
- PM sensor readings are fetched from `/api/sensors` endpoint
- AQI is calculated on the device based on actual PM2.5 readings
- Connection status indicator shows device connection state

## Integration Notes

1. **I2C Bus**: PM sensor shares the I2C bus with BME680 sensor
2. **Read Interval**: PM sensor is read every 3 seconds (same as other sensors)
3. **Backward Compatibility**: Existing sensor data (IAQ, temperature, humidity, etc.) remains unchanged
4. **Graceful Degradation**: System continues to work even if PM sensor is not detected
5. **Memory**: PM sensor driver uses minimal memory (~64 bytes per sensor instance)

## Building and Flashing

Standard ESP-IDF build process:
```bash
idf.py build
idf.py flash -p /dev/ttyUSB0
```

The PM sensor driver will be automatically compiled as part of the main component.

## Future Enhancements

Potential improvements:
- Add calibration for different altitudes
- Implement PM sensor self-diagnostics
- Add historical data logging and graphs
- Export PM and AQI data to external services
- Add email/SMS alerts for poor air quality
- Implement machine learning for predictive air quality
