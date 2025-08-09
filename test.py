from flask import Flask, render_template, request, jsonify
import serial
import serial.tools.list_ports
import threading
import time
from queue import Queue
import re

app = Flask(__name__)

# Global variables
serial_connection = None
serial_queue = Queue()
latest_state = {
    "NORTH": {"red": "0", "yellow": "0", "green": "0"},
    "SW": {"red": "0", "yellow": "0", "green": "0"},
    "SE": {"red": "0", "yellow": "0", "green": "0"},
    "NW": {"red": "0", "yellow": "0", "green": "0"},
    "NE": {"red": "0", "yellow": "0", "green": "0"}
}
arduino_commands = [
    "!order 0,1,2,3,4 - Set light sequence",
    "!delay 5000,2000,5000,... - Set all timings (15 values)",
    "!pause - Freeze current state",
    "!resume - Continue operation",
    "!status - Show current settings"
]

def find_arduino_port():
    """Try to find the Arduino port automatically"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if 'Arduino' in port.description or 'USB Serial Device' in port.description:
            return port.device
    return None

def parse_state_line(line):
    """Parse a STATE: message from Arduino into light states"""
    if not line.startswith("STATE:"):
        return None
    
    # Extract the state data
    state_data = line[6:]
    parts = state_data.split(',')
    
    # Create a dictionary to hold the parsed states
    parsed_states = {}
    
    # Process in chunks of 4 (light name, red, yellow, green)
    for i in range(0, len(parts), 4):
        if i + 3 >= len(parts):
            continue
            
        light_name = parts[i].strip().upper()
        red = parts[i+1].strip()
        yellow = parts[i+2].strip()
        green = parts[i+3].strip()
        
        # Store the state
        parsed_states[light_name] = {
            "red": red,
            "yellow": yellow,
            "green": green
        }
    
    return parsed_states

def serial_reader():
    """Thread function to continuously read from serial port"""
    global serial_connection, latest_state
    while True:
        if serial_connection and serial_connection.is_open:
            try:
                line = serial_connection.readline().decode('utf-8').strip()
                if line:
                    serial_queue.put(line)
                    
                    # Parse STATE messages to update light states
                    parsed_states = parse_state_line(line)
                    if parsed_states:
                        # Update latest state with the new data
                        for light, states in parsed_states.items():
                            if light in latest_state:
                                latest_state[light].update(states)
            except Exception as e:
                print(f"Serial read error: {e}")
                time.sleep(1)
        else:
            time.sleep(1)

@app.route('/')
def index():
    """Render the main page"""
    serial_output = "\n".join(list(serial_queue.queue)[-50:])  # Show last 50 lines
    return render_template(
        "traffic_light.html",
        serial_connected=serial_connection.is_open if serial_connection else False,
        port=serial_connection.port if serial_connection else "Not connected",
        commands=arduino_commands,
        serial_output=serial_output,
        is_paused=False  # track from Arduino messages
    )

@app.route('/send_command', methods=['POST'])
def send_command():
    """Send a command to the Arduino"""
    command = request.form.get('command', '').strip()
    if command and serial_connection and serial_connection.is_open:
        try:
            serial_connection.write((command + '\n').encode('utf-8'))
            return "Command sent"
        except Exception as e:
            return f"Error sending command: {e}"
    return "No command or serial not connected"

@app.route('/get_serial_output')
def get_serial_output():
    """Get the latest serial output"""
    return "\n".join(list(serial_queue.queue)[-50:])

@app.route('/get_state')
def get_state():
    """Get the latest light states"""
    # Ensure all light names are uppercase for consistency
    response = {k.upper(): v for k, v in latest_state.items()}
    return jsonify(response)

def main():
    """Main function to start the application"""
    global serial_connection
    
    # Try to find and connect to Arduino
    port = "COM7" 
    if port:
        try:
            serial_connection = serial.Serial(port, 115200, timeout=1)
            print(f"Connected to Arduino on {port}")
            # Wait for Arduino to initialize
            time.sleep(2)
        except Exception as e:
            print(f"Failed to connect to Arduino: {e}")
            serial_connection = None
    else:
        print("Arduino not found. Please connect it and restart the application.")
    
    # Start serial reader thread
    thread = threading.Thread(target=serial_reader, daemon=True)
    thread.start()
    
    # Start Flask app
    app.run(host='0.0.0.0', port=5000, debug=False)

if __name__ == '__main__':

    main()
