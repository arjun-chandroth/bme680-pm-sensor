# Quick Start Guide - Air Purifier Monitor

## Summary

Your air purifier monitoring system is now ready! Here's everything you need to know:

---

## **What You Have**

### **Firmware (ESP32)**
- Located: `/home/arjunc/aer.pm2.5/bme680_test/main/bme680_test.c`
- Reads all sensors and serves data via HTTP
- Includes WiFi connectivity
- Auto-updates all data every 3 seconds

### **Web UI**
- Located: `/home/arjunc/aer.pm2.5/bme680_test/web-ui/`
- Beautiful responsive dashboard
- Shows both intake and exhaust data
- Works on any device (phone, tablet, computer)

---

## **Step 1: Configure ESP32 WiFi**

Edit the WiFi credentials in your code before building:

**File**: `main/bme680_test.c` (around line 155)

```c
#define WIFI_SSID      "YOUR_SSID"        // Change this to your WiFi name
#define WIFI_PASSWORD  "YOUR_PASSWORD"    // Change this to your WiFi password
```

---

## **Step 2: Build & Flash ESP32**

```bash
cd /home/arjunc/aer.pm2.5/bme680_test

# Build
idf.py build

# Flash (connect USB cable first)
idf.py -p /dev/ttyUSB0 flash monitor
```

**Watch for this message in serial output:**
```
I (1234) BME680: WiFi connected! IP: 192.168.x.x
```

**Copy that IP address** - you'll need it in the next step.

---

## **Step 3: Open the Web UI**

### **Option A: Direct File Open (Simplest)**
1. Open file manager
2. Navigate to: `/home/arjunc/aer.pm2.5/bme680_test/web-ui/`
3. Double-click `index.html`
4. It opens in your default browser

### **Option B: Local Web Server**
```bash
cd /home/arjunc/aer.pm2.5/bme680_test/web-ui
python3 -m http.server 8000
```
Then open: `http://localhost:8000/index.html`

---

## **Step 4: Connect to ESP32**

Once the UI loads:

1. **Click the ⚙️ button** (top right, next to "Connecting...")
2. **Enter your ESP32's IP address**
   - Example: `192.168.1.100` (from Step 2)
3. **Click OK**
4. The page will attempt to connect

**Expected result**: 
- Status should change from "Using Simulated Data" → "Connected to ESP32"
- Live sensor readings will appear

---

## **Data Display**

### **Intake Tab** (Your ESP32 Sensors)
- ✅ **Air Quality Score** - IAQ gauge (0-500)
- ✅ **H₂S Sensor** - Raw ADC value & voltage
- ✅ **Odor Sensor** - Raw ADC value & voltage
- ✅ **Temperature** - From BME680
- ✅ **Humidity** - From BME680
- ✅ **Pressure** - From BME680
- ✅ **eCO₂** - Estimated CO2 equivalent
- ✅ **bVOC** - Breath VOC equivalent
- ✅ **Gas Resistance** - BME680 gas sensor

### **Exhaust Tab** (Currently Simulated - Placeholder)
- Same layout as Intake
- Currently shows simulated "cleaner" air (50% reduction)
- Ready for real exhaust sensor data later

---

## **Troubleshooting**

### Problem: "Using Simulated Data" Status Doesn't Change

**Solutions:**
1. ✅ Double-check the IP address (click ⚙️ to verify)
2. ✅ Make sure ESP32 is powered on
3. ✅ Verify both are on the same WiFi network
4. ✅ Check firewall settings (port 80 should be allowed)
5. ✅ Restart ESP32 by pressing the reset button

### Problem: Can't Find ESP32's IP Address

**Check serial monitor:**
```bash
idf.py -p /dev/ttyUSB0 monitor
```

Look for: `WiFi connected! IP: 192.168.x.x`

### Problem: WiFi Connection Fails

**Check:**
- SSID and password are spelled correctly
- Your WiFi is 2.4GHz (not 5GHz only)
- No special characters in the password (if so, modify code)

### Problem: Sensors Show All Zeros

**Check:**
- All sensor wires are properly connected
- I2C pins (21/22) and ADC pins (34/35) are correct
- Sensors have power (3.3V)

---

## **Architecture**

```
┌─────────────────────────────────┐
│     Your Computer/Phone         │
│   ┌──────────────────────────┐  │
│   │   Web Browser            │  │
│   │  index.html + app.js     │  │
│   └──────────────────────────┘  │
└────────────────────┬────────────┘
                     │ HTTP GET /api/sensors
                     │ (every 3 seconds)
                     │
┌────────────────────▼────────────┐
│      ESP32 NodeMCU             │
│   ┌──────────────────────────┐  │
│   │ HTTP Server (port 80)    │  │
│   ├──────────────────────────┤  │
│   │ WiFi: Connected          │  │
│   └──────────────────────────┘  │
│                                  │
│   ┌──────────────────────────┐  │
│   │ Sensors                  │  │
│   ├──────────────────────────┤  │
│   │ BME680 (I2C: 21, 22)    │  │
│   │ H2S (ADC: GPIO 34)       │  │
│   │ Odor (ADC: GPIO 35)      │  │
│   └──────────────────────────┘  │
└────────────────────────────────┘
```

---

## **Features**

✅ **Real-time Monitoring** - Updates every 3 seconds  
✅ **Beautiful UI** - Modern design with animations  
✅ **Responsive** - Works on desktop, tablet, phone  
✅ **No Installation** - Just open HTML file  
✅ **Intake & Exhaust** - Compare air quality before/after purifier  
✅ **Automatic IP Save** - Remember your ESP32 IP  
✅ **Fallback Data** - Shows simulated data if ESP32 offline  

---

## **Next Steps (For Later)**

### **Add Exhaust Sensor**
1. Connect another set of BME680 or gas sensors
2. Modify code to read from exhaust pins
3. Update `exhaustData` in `app.js` to fetch real data

### **Add Data Logging**
- Store historical data in ESP32 SPIFFS or SD card
- Display graphs showing trends over time

### **Mobile App**
- Convert to React Native or Flutter app
- Push notifications for poor air quality

### **Cloud Integration**
- Send data to AWS/Google Cloud
- View from anywhere in the world
- Set up automated alerts

---

## **Files Modified**

| File | Changes |
|------|---------|
| `main/bme680_test.c` | Added HTTP server, WiFi, sensor data storage |
| `main/CMakeLists.txt` | Added esp_http_server, esp_wifi, cjson dependencies |
| `web-ui/index.html` | Updated with Intake/Exhaust tabs |
| `web-ui/styles.css` | Added IP config button styling |
| `web-ui/app.js` | Complete rewrite for dual monitoring |

---

## **Support**

If something doesn't work:

1. **Check Serial Monitor**: `idf.py monitor`
2. **Check Browser Console**: Press F12 in browser
3. **Test Network**: Ping ESP32 from your computer
4. **Reset Everything**: Restart ESP32 and refresh browser

---

**Status**: ✅ Ready to Use!

**Next Task**: Set WiFi credentials and flash ESP32

