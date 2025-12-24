// Sensor data for intake and exhaust
let intakeData = {
    iaq: 50, staticIAQ: 50, eCO2: 400, bVOC: 0.5,
    temperature: 24.5, humidity: 45, pressure: 1013.25,
    gasResistance: 50000, h2sRaw: 0, h2sVoltage: 0,
    odorRaw: 0, odorVoltage: 0, stabilization: 50, runIn: 75,
    compTemp: 25.2, compHum: 46.5,
    pm1_0: 10, pm2_5: 25, pm10: 40, aqi: 60, aqi_level: "Moderate"
};

let exhaustData = {
    iaq: 30, staticIAQ: 30, eCO2: 380, bVOC: 0.2,
    temperature: 26.0, humidity: 42, pressure: 1013.25,
    gasResistance: 80000, h2sRaw: 500, h2sVoltage: 0.4,
    odorRaw: 400, odorVoltage: 0.3, stabilization: 90, runIn: 95,
    compTemp: 26.5, compHum: 43.0,
    pm1_0: 3, pm2_5: 8, pm10: 12, aqi: 30, aqi_level: "Good"
};

let isConnected = false;
let updateInterval = 3;

document.addEventListener('DOMContentLoaded', () => {
    initializeTabs();
    initializeData();
    setupAutoUpdate();
    updateClock();
    setInterval(updateClock, 1000);
    connectToSerialBridge();
});

// ============== SERIAL BRIDGE CONNECTION ==============
function connectToSerialBridge() {
    fetchSensorData();
}

// ============== TAB HANDLING ==============
function initializeTabs() {
    const tabButtons = document.querySelectorAll('.tab-button');
    const tabContents = document.querySelectorAll('.tab-content');
    tabButtons.forEach(button => {
        button.addEventListener('click', () => {
            const tabName = button.getAttribute('data-tab');
            tabButtons.forEach(btn => btn.classList.remove('active'));
            tabContents.forEach(content => content.classList.remove('active'));
            button.classList.add('active');
            document.getElementById(tabName).classList.add('active');
        });
    });
}

// ============== DATA INITIALIZATION ==============
function initializeData() {
    updateAllDisplay();
}

// ============== SIMULATED DATA UPDATES ==============
function simulateData() {
    setInterval(() => {
        intakeData.temperature += (Math.random() - 0.5) * 0.3;
        intakeData.temperature = Math.max(20, Math.min(30, intakeData.temperature));
        intakeData.humidity += (Math.random() - 0.5) * 2;
        intakeData.humidity = Math.max(30, Math.min(70, intakeData.humidity));
        intakeData.h2sRaw = Math.floor(Math.random() * 2048);
        intakeData.h2sVoltage = (intakeData.h2sRaw * 3.3 / 4095).toFixed(3);
        intakeData.odorRaw = Math.floor(Math.random() * 2048);
        intakeData.odorVoltage = (intakeData.odorRaw * 3.3 / 4095).toFixed(3);
        intakeData.iaq += (Math.random() - 0.5) * 5;
        intakeData.iaq = Math.max(0, Math.min(500, intakeData.iaq));
        intakeData.eCO2 += (Math.random() - 0.5) * 20;
        intakeData.eCO2 = Math.max(400, Math.min(5000, intakeData.eCO2));
        intakeData.bVOC += (Math.random() - 0.5) * 0.05;
        intakeData.bVOC = Math.max(0, Math.min(5, intakeData.bVOC));
        intakeData.gasResistance += (Math.random() - 0.5) * 5000;
        intakeData.gasResistance = Math.max(1000, Math.min(100000, intakeData.gasResistance));
        
        // PM data simulation
        intakeData.pm1_0 += (Math.random() - 0.5) * 3;
        intakeData.pm1_0 = Math.max(0, Math.min(500, intakeData.pm1_0));
        intakeData.pm2_5 += (Math.random() - 0.5) * 4;
        intakeData.pm2_5 = Math.max(0, Math.min(500, intakeData.pm2_5));
        intakeData.pm10 += (Math.random() - 0.5) * 5;
        intakeData.pm10 = Math.max(0, Math.min(500, intakeData.pm10));
        
        // Calculate AQI based on PM2.5
        calculateAQI(intakeData);

        exhaustData.temperature = intakeData.temperature - 1;
        exhaustData.humidity = intakeData.humidity - 3;
        exhaustData.h2sRaw = Math.floor(intakeData.h2sRaw * 0.3);
        exhaustData.h2sVoltage = (exhaustData.h2sRaw * 3.3 / 4095).toFixed(3);
        exhaustData.odorRaw = Math.floor(intakeData.odorRaw * 0.25);
        exhaustData.odorVoltage = (exhaustData.odorRaw * 3.3 / 4095).toFixed(3);
        exhaustData.iaq = intakeData.iaq * 0.5;
        exhaustData.eCO2 = intakeData.eCO2 * 0.8;
        exhaustData.bVOC = intakeData.bVOC * 0.4;
        exhaustData.gasResistance = intakeData.gasResistance * 2;
        
        // Exhaust PM data (reduced)
        exhaustData.pm1_0 = intakeData.pm1_0 * 0.3;
        exhaustData.pm2_5 = intakeData.pm2_5 * 0.3;
        exhaustData.pm10 = intakeData.pm10 * 0.3;
        
        // Calculate AQI for exhaust
        calculateAQI(exhaustData);

        updateAllDisplay();
    }, updateInterval * 1000);
}

// ============== AQI CALCULATION ==============
function calculateAQI(dataSet) {
    const pm25 = dataSet.pm2_5;
    
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

// ============== UPDATE ALL DISPLAY ==============
function updateAllDisplay() {
    updateIntakeSensors();
    updateExhaustSensors();
}

function updateIntakeSensors() {
    const iaqScore = Math.round(intakeData.iaq);
    document.getElementById('intakeIaqScore').textContent = iaqScore;
    
    let status = 'Excellent';
    let statusClass = '';
    if (iaqScore >= 50 && iaqScore < 100) status = 'Good';
    else if (iaqScore >= 100 && iaqScore < 150) { status = 'Lightly Polluted'; statusClass = 'moderate'; }
    else if (iaqScore >= 150 && iaqScore < 200) { status = 'Moderately Polluted'; statusClass = 'moderate'; }
    else if (iaqScore >= 200 && iaqScore < 300) { status = 'Heavily Polluted'; statusClass = 'poor'; }
    else if (iaqScore >= 300) { status = 'Severely Polluted'; statusClass = 'poor'; }
    
    const statusEl = document.getElementById('intakeIaqStatus');
    statusEl.textContent = status;
    statusEl.className = `iaq-status ${statusClass}`;
    
    const angle = (iaqScore / 500) * 180 - 90;
    document.getElementById('intakeIaqNeedle').setAttribute('transform', `translate(100, 100) rotate(${angle})`);
    
    document.getElementById('intakeTemp').textContent = intakeData.temperature.toFixed(1);
    document.getElementById('intakeHum').textContent = intakeData.humidity.toFixed(1);
    document.getElementById('intakePres').textContent = intakeData.pressure.toFixed(1);
    
    document.getElementById('intakeH2sRaw').textContent = intakeData.h2sRaw;
    document.getElementById('intakeH2sVolt').textContent = intakeData.h2sVoltage;
    document.getElementById('intakeH2sBar').style.width = (intakeData.h2sRaw / 4095 * 100) + '%';
    
    document.getElementById('intakeOdorRaw').textContent = intakeData.odorRaw;
    document.getElementById('intakeOdorVolt').textContent = intakeData.odorVoltage;
    document.getElementById('intakeOdorBar').style.width = (intakeData.odorRaw / 4095 * 100) + '%';
    
    document.getElementById('intakeEco2').textContent = Math.round(intakeData.eCO2);
    document.getElementById('intakeBvoc').textContent = intakeData.bVOC.toFixed(2);
    document.getElementById('intakeGas').textContent = Math.round(intakeData.gasResistance).toLocaleString();
    
    document.getElementById('intakePm1').textContent = Math.round(intakeData.pm1_0);
    document.getElementById('intakePm25').textContent = Math.round(intakeData.pm2_5);
    document.getElementById('intakePm10').textContent = Math.round(intakeData.pm10);
    
    document.getElementById('intakeAqiScore').textContent = Math.round(intakeData.aqi);
    const aqiLevelEl = document.getElementById('intakeAqiLevel');
    aqiLevelEl.textContent = intakeData.aqi_level;
    aqiLevelEl.className = 'value-lg aqi-level ' + getAQIClass(intakeData.aqi_level);
    
    document.getElementById('intakeStab').style.width = intakeData.stabilization + '%';
    document.getElementById('intakeStabVal').textContent = Math.round(intakeData.stabilization) + '%';
    document.getElementById('intakeRunIn').style.width = intakeData.runIn + '%';
    document.getElementById('intakeRunInVal').textContent = Math.round(intakeData.runIn) + '%';
}

// ============== GET AQI COLOR CLASS ==============
function getAQIClass(level) {
    if (level === "Good") return "aqi-good";
    if (level === "Moderate") return "aqi-moderate";
    if (level.includes("Unhealthy for Sensitive")) return "aqi-sensitive";
    if (level === "Unhealthy") return "aqi-unhealthy";
    if (level === "Very Unhealthy") return "aqi-very-unhealthy";
    if (level === "Hazardous") return "aqi-hazardous";
    return "";
}

function updateExhaustSensors() {
    const iaqScore = Math.round(exhaustData.iaq);
    document.getElementById('exhaustIaqScore').textContent = iaqScore;
    
    let status = 'Excellent';
    let statusClass = '';
    if (iaqScore >= 50 && iaqScore < 100) status = 'Good';
    else if (iaqScore >= 100 && iaqScore < 150) { status = 'Lightly Polluted'; statusClass = 'moderate'; }
    else if (iaqScore >= 150 && iaqScore < 200) { status = 'Moderately Polluted'; statusClass = 'moderate'; }
    else if (iaqScore >= 200 && iaqScore < 300) { status = 'Heavily Polluted'; statusClass = 'poor'; }
    else if (iaqScore >= 300) { status = 'Severely Polluted'; statusClass = 'poor'; }
    
    const statusEl = document.getElementById('exhaustIaqStatus');
    statusEl.textContent = status;
    statusEl.className = `iaq-status ${statusClass}`;
    
    const angle = (iaqScore / 500) * 180 - 90;
    document.getElementById('exhaustIaqNeedle').setAttribute('transform', `translate(100, 100) rotate(${angle})`);
    
    document.getElementById('exhaustTemp').textContent = exhaustData.temperature.toFixed(1);
    document.getElementById('exhaustHum').textContent = exhaustData.humidity.toFixed(1);
    document.getElementById('exhaustPres').textContent = exhaustData.pressure.toFixed(1);
    
    document.getElementById('exhaustH2sRaw').textContent = exhaustData.h2sRaw;
    document.getElementById('exhaustH2sVolt').textContent = exhaustData.h2sVoltage;
    document.getElementById('exhaustH2sBar').style.width = (exhaustData.h2sRaw / 4095 * 100) + '%';
    
    document.getElementById('exhaustOdorRaw').textContent = exhaustData.odorRaw;
    document.getElementById('exhaustOdorVolt').textContent = exhaustData.odorVoltage;
    document.getElementById('exhaustOdorBar').style.width = (exhaustData.odorRaw / 4095 * 100) + '%';
    
    document.getElementById('exhaustEco2').textContent = Math.round(exhaustData.eCO2);
    document.getElementById('exhaustBvoc').textContent = exhaustData.bVOC.toFixed(2);
    document.getElementById('exhaustGas').textContent = Math.round(exhaustData.gasResistance).toLocaleString();
    
    document.getElementById('exhaustPm1').textContent = Math.round(exhaustData.pm1_0);
    document.getElementById('exhaustPm25').textContent = Math.round(exhaustData.pm2_5);
    document.getElementById('exhaustPm10').textContent = Math.round(exhaustData.pm10);
    
    document.getElementById('exhaustAqiScore').textContent = Math.round(exhaustData.aqi);
    const exhaustAqiLevelEl = document.getElementById('exhaustAqiLevel');
    exhaustAqiLevelEl.textContent = exhaustData.aqi_level;
    exhaustAqiLevelEl.className = 'value-lg aqi-level ' + getAQIClass(exhaustData.aqi_level);
    
    document.getElementById('exhaustStab').style.width = exhaustData.stabilization + '%';
    document.getElementById('exhaustStabVal').textContent = Math.round(exhaustData.stabilization) + '%';
    document.getElementById('exhaustRunIn').style.width = exhaustData.runIn + '%';
    document.getElementById('exhaustRunInVal').textContent = Math.round(exhaustData.runIn) + '%';
}

// ============== SETUP AUTO UPDATE ==============
function setupAutoUpdate() {
    setConnectionStatus(false);
    simulateData();
}

// ============== CONNECTION STATUS ==============
function setConnectionStatus(connected) {
    isConnected = connected;
    const statusDot = document.getElementById('connectionStatus');
    const statusText = document.getElementById('connectionText');
    
    if (connected) {
        statusDot.classList.remove('disconnected');
        statusDot.classList.add('connected');
        statusText.textContent = 'Connected to ESP32';
    } else {
        statusDot.classList.remove('connected');
        statusDot.classList.add('disconnected');
        statusText.textContent = 'Using Simulated Data';
    }
}

// ============== CLOCK UPDATE ==============
function updateClock() {
    const now = new Date();
    const timeString = now.toLocaleTimeString('en-US', { 
        hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit'
    });
    document.getElementById('lastUpdate').textContent = timeString;
}

// ============== SENSOR DATA FETCHING ==============
function fetchSensorData() {
    const apiUrl = 'http://localhost:8888/api/sensors';
    
    fetch(apiUrl, { mode: 'cors', method: 'GET' })
        .then(response => response.json())
        .then(data => {
            intakeData = {
                iaq: data.iaq || intakeData.iaq,
                staticIAQ: data.static_iaq || intakeData.staticIAQ,
                eCO2: data.eco2 || intakeData.eCO2,
                bVOC: data.bvoc || intakeData.bVOC,
                temperature: data.temperature || intakeData.temperature,
                humidity: data.humidity || intakeData.humidity,
                pressure: data.pressure || intakeData.pressure,
                gasResistance: data.gas_resistance || intakeData.gasResistance,
                h2sRaw: data.h2s_raw || intakeData.h2sRaw,
                h2sVoltage: data.h2s_voltage || intakeData.h2sVoltage,
                odorRaw: data.odor_raw || intakeData.odorRaw,
                odorVoltage: data.odor_voltage || intakeData.odorVoltage,
                stabilization: data.stabilization || intakeData.stabilization,
                runIn: data.run_in || intakeData.runIn,
                compTemp: data.comp_temp || intakeData.compTemp,
                compHum: data.comp_hum || intakeData.compHum,
                pm1_0: data.pm1_0 || intakeData.pm1_0,
                pm2_5: data.pm2_5 || intakeData.pm2_5,
                pm10: data.pm10 || intakeData.pm10,
                aqi: data.aqi || intakeData.aqi,
                aqi_level: data.aqi_level || intakeData.aqi_level
            };
            
            // Exhaust data is reduced percentage of intake
            exhaustData.iaq = intakeData.iaq * 0.5;
            exhaustData.staticIAQ = intakeData.staticIAQ * 0.5;
            exhaustData.eCO2 = intakeData.eCO2 * 0.8;
            exhaustData.bVOC = intakeData.bVOC * 0.4;
            exhaustData.temperature = intakeData.temperature - 1;
            exhaustData.humidity = intakeData.humidity - 3;
            exhaustData.pressure = intakeData.pressure;
            exhaustData.gasResistance = intakeData.gasResistance * 2;
            exhaustData.h2sRaw = Math.floor(intakeData.h2sRaw * 0.3);
            exhaustData.h2sVoltage = (exhaustData.h2sRaw * 3.3 / 4095).toFixed(3);
            exhaustData.odorRaw = Math.floor(intakeData.odorRaw * 0.25);
            exhaustData.odorVoltage = (exhaustData.odorRaw * 3.3 / 4095).toFixed(3);
            exhaustData.stabilization = intakeData.stabilization;
            exhaustData.runIn = intakeData.runIn;
            exhaustData.compTemp = intakeData.compTemp;
            exhaustData.compHum = intakeData.compHum;
            
            // PM data reduction for exhaust
            exhaustData.pm1_0 = intakeData.pm1_0 * 0.3;
            exhaustData.pm2_5 = intakeData.pm2_5 * 0.3;
            exhaustData.pm10 = intakeData.pm10 * 0.3;
            
            // Recalculate AQI for exhaust data
            calculateAQI(exhaustData);
            
            setConnectionStatus(true);
            updateAllDisplay();
        })
        .catch(error => {
            console.error('Failed to fetch sensor data:', error);
            setConnectionStatus(false);
        });
    
    setInterval(fetchSensorData, 3000);
}
