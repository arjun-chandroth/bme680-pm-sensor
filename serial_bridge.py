#!/usr/bin/env python3
"""
Serial Bridge: Reads JSON from ESP32 serial port and serves via HTTP
"""
import serial
import json
import threading
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse

PORT = 8888
BAUD_RATE = 115200
SERIAL_PORT = "/dev/ttyUSB0"

# Latest sensor data
latest_data = {}
data_lock = threading.Lock()

class SerialReader(threading.Thread):
    def __init__(self, port, baud_rate):
        super().__init__(daemon=True)
        self.port = port
        self.baud_rate = baud_rate
        self.running = True
    
    def run(self):
        global latest_data
        try:
            ser = serial.Serial(self.port, self.baud_rate, timeout=1)
            print(f"Connected to {self.port} at {self.baud_rate} baud")
            
            while self.running:
                try:
                    raw_line = ser.readline()
                    try:
                        line = raw_line.decode('utf-8', errors='ignore').strip()
                    except:
                        continue
                        
                    if line.startswith('{') and line.endswith('}'):
                        try:
                            data = json.loads(line)
                            with data_lock:
                                latest_data = data
                            print(f"Updated: {len(latest_data)} fields")
                        except json.JSONDecodeError:
                            pass
                except Exception as e:
                    time.sleep(0.1)
        except serial.SerialException as e:
            print(f"Serial port error: {e}")
            print(f"Make sure ESP32 is connected to {self.port}")

class SensorHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        path = urlparse(self.path).path
        
        if path == '/api/sensors':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            
            with data_lock:
                response = json.dumps(latest_data)
            self.wfile.write(response.encode())
        else:
            self.send_response(404)
            self.end_headers()
    
    def log_message(self, format, *args):
        # Suppress default logging
        pass

if __name__ == '__main__':
    # Start serial reader
    reader = SerialReader(SERIAL_PORT, BAUD_RATE)
    reader.start()
    
    # Start HTTP server
    server = HTTPServer(('localhost', PORT), SensorHandler)
    print(f"HTTP server running on http://localhost:{PORT}")
    print(f"Sensor data: http://localhost:{PORT}/api/sensors")
    print(f"Press Ctrl+C to stop")
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down...")
        reader.running = False
        server.shutdown()
