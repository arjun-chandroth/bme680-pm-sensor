#!/usr/bin/env python3
import serial
import json
import threading
import time
import glob
from http.server import HTTPServer, BaseHTTPRequestHandler

latest_data = {}
data_lock = threading.Lock()
connection_status = {"connected": False}

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/api/sensors':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            with data_lock:
                response = dict(latest_data)
                response['connected'] = connection_status['connected']
                self.wfile.write(json.dumps(response).encode())
        else:
            self.send_response(404)
            self.end_headers()
    
    def log_message(self, *args):
        pass

def find_esp32_port():
    """Find available serial ports, preferring /dev/ttyUSB*"""
    try:
        # Look for common ESP32 ports
        ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyACM*') + glob.glob('/dev/cu.usbserial*')
        if ports:
            return sorted(ports)[0]  # Return first available
    except:
        pass
    return None

def read_serial():
    global latest_data
    last_port = None
    
    while True:
        try:
            port = find_esp32_port()
            
            if not port:
                if last_port:
                    print(f"✗ ESP32 disconnected (port {last_port} not found)")
                    last_port = None
                connection_status['connected'] = False
                time.sleep(1)
                continue
            
            if port != last_port:
                print(f"✓ Found ESP32 on {port}")
                last_port = port
            
            ser = serial.Serial(port, 115200, timeout=2)
            print(f"✓ Connected to ESP32 on {port}")
            connection_status['connected'] = True
            
            while True:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line.startswith('{') and line.endswith('}'):
                        data = json.loads(line)
                        with data_lock:
                            latest_data = data
                except json.JSONDecodeError:
                    pass
                except Exception as e:
                    print(f"Read error: {e}")
                    break
        except serial.SerialException as e:
            connection_status['connected'] = False
            print(f"✗ Serial error: {e}")
            time.sleep(1)
        except Exception as e:
            connection_status['connected'] = False
            print(f"✗ Error: {e}")
            time.sleep(1)

if __name__ == '__main__':
    threading.Thread(target=read_serial, daemon=True).start()
    server = HTTPServer(('localhost', 8888), Handler)
    print("API: http://localhost:8888/api/sensors")
    print("Auto-detecting ESP32 on serial ports...")
    print("Press Ctrl+C to stop")
    server.serve_forever()
