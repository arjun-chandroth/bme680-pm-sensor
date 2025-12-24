# Air Purifier Monitor UI

Complete web-based monitoring system for your air purifier intake and exhaust air quality.

## Features

- **Real-time Sensor Data**: Display BME680, H2S, and Odor sensor readings
- **Dual Monitoring**: Separate intake and exhaust air quality monitoring
- **Live Dashboard**: Beautiful, responsive UI with gauges and graphs
- **ESP32 Integration**: Automatic data fetching from your ESP32
- **Simulated Mode**: Works with simulated data before ESP32 is connected

## Getting Started

### 1. Open the UI

Simply open `index.html` in any modern web browser:
- **Local file**: Open `/web-ui/index.html` directly in your browser
- **Via HTTP Server**: Host on a local web server if needed

```bash
# Python 3
python3 -m http.server 8000

# Then visit: http://localhost:8000/web-ui/index.html
```

### 2. Configure ESP32 IP Address

Click the **⚙️ (settings)** button next to "Connecting..." in the top-right corner and enter your ESP32's IP address.

**Example IP addresses**:
- `192.168.1.100` (common home network)
- `192.168.0.50` (alternative network)
- Check your router's DHCP clients or use the ESP32 serial monitor to find the IP

The IP address is saved to browser local storage, so you only need to enter it once.

### 3. ESP32 Configuration

The ESP32 firmware must have:

1. **WiFi Credentials** - Edit these in `main/bme680_test.c`:
   ```c
   #define WIFI_SSID      "YOUR_SSID"        // Your WiFi network name
   #define WIFI_PASSWORD  "YOUR_PASSWORD"    // Your WiFi password
   ```

2. **HTTP Server** - Already included in the code
   - Listens on port **80**
   - Provides JSON data at `/api/sensors`

3. **Recompile and Flash**:
   ```bash
   cd /home/arjunc/aer.pm2.5/bme680_test
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

## Data Flow

### **Intake Data** (Coming from ESP32)
- **Source**: BME680 sensor (I2C on pins 21/22)
- **Readings**: Temperature, Humidity, Pressure, IAQ, eCO2, bVOC, Gas Resistance
- **Gas Sensors**: H2S (pin 34), Odor (pin 35)

### **Exhaust Data** (Currently Simulated)
- Simulates cleaner air than intake (50% pollution reduction)
- Ready for real exhaust sensor setup later
- Same metrics as intake

## API Endpoint

### GET `/api/sensors`

Returns JSON with all sensor data:

```json
{
  "iaq": 50.5,
  "iaq_accuracy": 3,
  "static_iaq": 48.2,
  "eco2": 410.5,
  "bvoc": 0.5,
  "temperature": 24.5,
  "humidity": 45.2,
  "pressure": 1013.25,
  "gas_resistance": 50000,
  "h2s_raw": 1024,
  "h2s_voltage": 0.829,
  "odor_raw": 512,
  "odor_voltage": 0.414,
  "stabilization": 50.0,
  "run_in": 75.0,
  "comp_temp": 25.2,
  "comp_hum": 46.5
}
```

## Troubleshooting

### UI Shows "Using Simulated Data"
- The web page cannot connect to your ESP32
- **Check**:
  1. ESP32 IP address is correct (click ⚙️ to change)
  2. ESP32 is connected to the same WiFi network
  3. Both devices are on the same network (not on different networks)
  4. Firewall isn't blocking port 80
  5. ESP32 serial monitor shows WiFi connected message

### Data Isn't Updating
- Try clicking the ⚙️ button and re-entering the IP address
- Or hard-refresh the browser (Ctrl+F5 or Cmd+Shift+R)
- Check that all sensors are properly connected to the ESP32

### How to Find ESP32 IP Address
1. **Via Serial Monitor**: Flash the code and watch the serial output:
   ```
   I (1234) BME680: WiFi connected! IP: 192.168.1.xxx
   ```
2. **Via Router**: Log into your WiFi router's admin page and check connected devices
3. **Via Phone/Computer**: Use network scanning tools like Angry IP Scanner

## File Structure

```
web-ui/
├── index.html      # Main UI page
├── styles.css      # All styling
├── app.js          # JavaScript logic
└── README.md       # This file
```

## Browser Compatibility

- Chrome/Chromium (recommended)
- Firefox
- Safari
- Edge
- Any modern browser supporting ES6

## Customization

### Change Update Interval
Edit in `app.js`:
```javascript
let updateInterval = 3; // seconds (default: 3)
```

### Change Default IP
Edit in `app.js`:
```javascript
let esp32Ip = localStorage.getItem('esp32_ip') || '192.168.1.100';
```

### Modify Gauge Ranges
Edit the gauge logic in `updateIntakeSensors()` and `updateExhaustSensors()` functions.

## Adding Exhaust Data Later

When you have exhaust sensors connected to another ESP32 or pins:

1. Modify the exhaust data fetch to get real data instead of simulated
2. Create a second `/api/exhaust` endpoint or modify the `connectToESP32()` function
3. The UI structure is already ready for real exhaust data

## Support

For issues with:
- **UI**: Check browser console (F12) for JavaScript errors
- **Connection**: Verify network connectivity and IP address
- **Sensors**: Check ESP32 serial monitor for sensor initialization messages

---

**Last Updated**: December 24, 2025
