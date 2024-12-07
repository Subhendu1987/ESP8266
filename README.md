# ESP8266 Home Automation - Dynamic Setup

A fully dynamic ESP8266-based home automation system that allows users to control appliances using IR remotes, manual switches, or a web interface. The system also includes a DHT11 sensor for monitoring room temperature and humidity. No hardcoded credentials are required, making it flexible and easy to set up.

 ![Image Alt](https://github.com/Subhendu1987/ESP8266/blob/0783b71cf12fec986d2e85ed57b445af37956346/dashboard.jpg)


## Features

- **Dynamic Configuration**: 
  - Configure Wi-Fi and MQTT credentials at runtime—no need to re-upload the code to update them.
  - Set room names and switch names dynamically.

- **Relay Controls**:
  - Control up to 4 relays using:
    - IR remote signals.
    - Manual rocker switches.
    - A web-based interface.

- **DHT11 Sensor**:
  - Monitor room temperature and humidity via MQTT or web interface.

- **MQTT Integration**:
  - Securely publish and subscribe to relay states and DHT11 sensor data.
  - Compatible with HiveMQ and other MQTT brokers.

- **State Persistence**:
  - Stores relay states and IR codes in EEPROM to ensure states are retained after a power cycle.

## Getting Started

### Hardware Requirements
- **ESP8266 board** (e.g., NodeMCU, Wemos D1 Mini).
- **DHT11 sensor** for temperature and humidity readings.
- **4 relays** for controlling appliances.
- **IR receiver module** for remote control functionality.
- **Manual rocker switches** for physical control of relays.
- Power supply, connecting wires, and breadboard or PCB.

### Software Requirements
- Arduino IDE with the following libraries:
  - `ESP8266WiFi`
  - `PubSubClient`
  - `EEPROM`
  - `DHT`
  - `IRremoteESP8266`

### Setup Instructions

1. **Flash the ESP8266**:
   - Upload the code to your ESP8266 using the Arduino IDE.
   - Ensure all required libraries are installed.

2. **Initial Setup**:
   - After flashing, connect to the ESP8266's Wi-Fi AP named `ESP_Config`.
   - Open a browser and navigate to `192.168.4.1`.
   - Enter your Wi-Fi credentials, MQTT broker details, and customize your room and switch names.

3. **Control the Relays**:
   - Use the web interface to toggle individual relays or control all relays simultaneously.
   - Use an IR remote for quick relay toggling.
   - Physical rocker switches allow manual control.

4. **Monitor Temperature and Humidity**:
   - View temperature and humidity data on the web interface or receive updates via MQTT.

## Usage

### Web Interface
Access the web interface by entering the ESP8266's IP address in your browser. The interface provides:
- Relay toggle buttons.
- Bulk control options.
- Temperature and humidity readings.

### MQTT Topics
- **Publish**:
  - `room/relay1/state`: Relay 1 state.
  - `room/relay2/state`: Relay 2 state.
  - `room/temperature`: Room temperature.
  - `room/humidity`: Room humidity.
- **Subscribe**:
  - `room/relay1/control`: Control Relay 1.
  - `room/relay2/control`: Control Relay 2.
  - `room/all/control`: Control all relays.

### IR Remote
Configure the IR remote by capturing IR codes during setup. Use the remote to toggle individual relays dynamically.

## Contributing
Contributions are welcome! Feel free to fork the repository, create new features, or report issues.

## License
This project is licensed under the MIT License. See the `LICENSE` file for details.

---

Happy Automating! 🎉
