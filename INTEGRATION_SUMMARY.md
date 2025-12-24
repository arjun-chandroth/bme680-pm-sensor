# PM2.5 Sensor Integration - Quick Reference

## What Was Added

### 1. PM Sensor Driver Files
- **DFRobot_AirQualitySensor.h** - Header with function declarations
- **DFRobot_AirQualitySensor.c** - I2C driver implementation for reading PM values

### 2. Firmware Changes (bme680_test.c)
- PM sensor initialization on I2C address 0x19
- `read_pm_sensor()` - Reads PM1.0, PM2.5, PM10 values
- `calculate_aqi()` - Computes EPA standard AQI from PM2.5
- JSON output now includes: `pm1_0`, `pm2_5`, `pm10`, `aqi`, `aqi_level`

### 3. Web UI Enhancements (index.html, app.js, styles.css)
- New "Particulate Matter (PM)" section showing PM1.0, PM2.5, PM10
- New "Air Quality Index (AQI)" section with score and descriptive level
- Color-coded AQI display (green=good, red=poor, purple=hazardous)
- Both Intake and Exhaust tabs display PM and AQI data

## Sensor Output Example

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

## AQI Color Reference

| Level | Color | PM2.5 Range | AQI Range |
|-------|-------|-------------|-----------|
| Good | 🟢 Green | 0-12 | 0-50 |
| Moderate | 🟡 Yellow | 12-35.4 | 51-100 |
| Unhealthy for Sensitive | 🟠 Orange | 35.5-55.4 | 101-150 |
| Unhealthy | 🔴 Red | 55.5-150.4 | 151-200 |
| Very Unhealthy | 🟣 Purple | 150.5-250.4 | 201-300 |
| Hazardous | 🔴 Dark Red | 250.5+ | 301+ |

## Testing

### Simulated Mode (No ESP32)
- Click "Intake Air" tab - shows simulated PM and AQI data
- PM values change every 3 seconds
- AQI automatically recalculates
- Connection indicator shows "Using Simulated Data"

### Real Hardware Mode
- ESP32 must serve `/api/sensors` endpoint with PM and AQI data
- Web UI fetches data every 3 seconds
- Connection indicator shows "Connected to ESP32"

## Key Functions Added

### C Code (bme680_test.c)
```c
read_pm_sensor()        // Reads all PM values
calculate_aqi()         // Calculates AQI from PM2.5
dfrobot_create()        // Initialize PM sensor
dfrobot_begin()         // Start PM sensor
dfrobot_gainParticleConcentration_ugm3()  // Read PM values
```

### JavaScript (app.js)
```javascript
calculateAQI(dataSet)   // Compute AQI from PM2.5
getAQIClass(level)      // Get CSS class for color coding
updateIntakeSensors()   // Updated with PM and AQI display
updateExhaustSensors()  // Updated with PM and AQI display
```

## Build & Flash

```bash
cd /home/arjunc/aer.pm2.5/bme680_test
idf.py build
idf.py flash -p /dev/ttyUSB0
idf.py monitor -p /dev/ttyUSB0
```

The PM sensor will be automatically integrated into the build.

## I2C Details

- **Address**: 0x19
- **Pins**: SDA=21, SCL=22
- **Registers**:
  - 0x00: Firmware version
  - 0x03: PM1.0 (atmospheric mode)
  - 0x04: PM2.5 (atmospheric mode)
  - 0x05: PM10 (atmospheric mode)

## Notes

✅ PM sensor shares I2C bus with BME680  
✅ Works with existing sensor data (no conflicts)  
✅ Graceful degradation if sensor not found  
✅ Both simulated and real data modes supported  
✅ AQI calculation follows US EPA standard  
✅ Exhaust air shows 30% of intake PM values (simulating filtration)  

## Files Modified Summary

1. ✨ NEW: `main/DFRobot_AirQualitySensor.h`
2. ✨ NEW: `main/DFRobot_AirQualitySensor.c`
3. 📝 MODIFIED: `main/bme680_test.c` - Added PM integration
4. 📝 MODIFIED: `main/CMakeLists.txt` - Added driver to build
5. 📝 MODIFIED: `web-ui/index.html` - Added PM and AQI sections
6. 📝 MODIFIED: `web-ui/app.js` - Added PM/AQI display and calculation
7. 📝 MODIFIED: `web-ui/styles.css` - Added AQI color styling
8. 📄 NEW: `PM_SENSOR_INTEGRATION.md` - Full integration documentation
