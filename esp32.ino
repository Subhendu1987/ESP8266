#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <IRremote.hpp> 
#include <DHT.h>
#include <MQTT.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>

int relayPin1 = 23;
int relayPin2 = 22;
int relayPin3 = 21;
int relayPin4 = 19;

int switchPin1 = 27;
int switchPin2 = 14;
int switchPin3 = 12;
int switchPin4 = 13;


int irPin = 4;
int dhtPin = 5;
int buzzerpin = 18;

// Current firmware version
const char* current_version = "1.0.0.2";
// Firmware filename
String filename = "Esp32_Firmware_1.0.0.3.bin";


#define SSID_ADDR 60         // Starting address for Wi-Fi SSID
#define PASS_ADDR 70         // Starting address for Wi-Fi Password
#define MQTT_SERVER_ADDR 100 // Starting address for MQTT Server
#define MQTT_PORT_ADDR 200   // Starting address for MQTT Port
#define MQTT_USER_ADDR 210   // Starting address for MQTT Username
#define MQTT_PASS_ADDR 250   // Starting address for MQTT Password
#define ROOM_NAME 300        // Starting address for roomname
#define RELAY_ONE 330        // for relay 1 name
#define RELAY_TWO 360        // for relay 2 name
#define RELAY_THREE 390      // for relay 3 name
#define RELAY_FOUR 420       // for relay 4 name

#define RESET_FLAG_ADDR 280
#define RESET_TIME_ADDR 284

#define DOUBLE_RESET_INTERVAL 5000 // 5 seconds (time window for the second reset)
unsigned long previousrestarMillis = 0;
const unsigned long resetinmls = 10800000; // 3 hours in milliseconds to schedule restart
const unsigned long dhtpublishinterval = 60000; // 1 minutes interval for publish dht data

bool setupmode = false;


// DNS Server
const byte DNS_PORT = 53;
DNSServer dnsServer;

#define BUZZER_PIN buzzerpin 

// Web server on port 80
WebServer server(80);

// MQTT client
WiFiClientSecure net;
MQTTClient client;

#define IR_RECEIVE_PIN irPin
decode_results results;

#define DHTPIN dhtPin
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
unsigned long lastDHTPublishTime = 0; // Stores the last publish time
const unsigned long DHTPublishInterval = 600000; // 5 minutes in milliseconds

bool relayStates1 = false;
bool relayStates2 = false;
bool relayStates3 = false;
bool relayStates4 = false;
bool relayStates5 = false;

bool previousSwitchStates1 = true;
bool previousSwitchStates2 = true;
bool previousSwitchStates3 = true;
bool previousSwitchStates4 = true;

bool isInSetMode = false;

int currentSetRelay = -1;

unsigned long retryInterval = 10000; // Retry interval in milliseconds
unsigned long lastRetryTime = 0;     // Timestamp of the last retry attempt
bool wifiConnected = false;
bool mqttsubscribe = false;

// Base URL for firmware files
const char* base_url = "https://raw.githubusercontent.com/Subhendu1987/ESP8266-ESP32-Home-Automation/refs/heads/main/";
// Dynamically construct the firmware URL
String firmware_url = String(base_url) + filename;
bool updatecheck = false;

void play() {
  for (int i = 0; i < 2; i++) { // Play the tone twice
    tone(BUZZER_PIN, 1500, 300); // Beep at 2kHz for 200ms
    delay(350);                  // Wait for 275ms between beeps
  }
  noTone(BUZZER_PIN);
}

void checkForOTAUpdate() {
  Serial.println("Checking for OTA update...");

  HTTPClient http;
  http.begin(firmware_url);

  // Check if the firmware URL exists
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Device is already up-to-date. No new firmware found.");
    http.end();
    updatecheck = true;
    return;
  }

  // Extract version from firmware_url
  String firmwareFileName = filename;
  String otaVersion = "";
  for (int i = 0; i < firmwareFileName.length(); i++) {
    char c = firmwareFileName.charAt(i);
    if (isdigit(c) || c == '.') {
      otaVersion += c;
    }
  }

  // Remove first two characters and the last character
  if (otaVersion.length() > 3) {
    otaVersion = otaVersion.substring(2, otaVersion.length() - 1);
  }

  Serial.println("Current firmware version: " + String(current_version));
  Serial.println("Available firmware version: " + otaVersion);

  // Compare versions
  if (strcmp(otaVersion.c_str(), current_version) == 0) {
    Serial.println("Device is already up-to-date.");
    http.end();
    updatecheck = true;
    return;
  }

  Serial.println("New firmware version detected. Starting OTA update...");
  performOTAUpdate(firmware_url.c_str());
}

void performOTAUpdate(const char* firmwareURL) {
  updatecheck = true;
  HTTPClient http;
  http.begin(firmwareURL);

  Serial.println("Downloading firmware...");

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    if (contentLength > 0) {
      if (!Update.begin(contentLength)) {
        Serial.println("Not enough space to begin OTA");
        return;
      }

      WiFiClient* client = http.getStreamPtr();
      size_t written = Update.writeStream(*client);
      if (written == contentLength) {
        Serial.println("OTA written successfully.");
      } else {
        Serial.printf("OTA written %d/%d bytes.\n", written, contentLength);
      }

      if (Update.end()) {
        Serial.println("OTA update finished successfully.");
        if (Update.isFinished()) {
          Serial.println("Rebooting...");
          ESP.restart();
        } else {
          Serial.println("Update not finished? Something went wrong.");
        }
      } else {
        Serial.printf("Error Occurred: %s\n", Update.errorString());
      }
    } else {
      Serial.println("Content length is not valid.");
    }
  } else {
    Serial.printf("Failed to download firmware. HTTP code: %d\n", httpCode);
  }
  http.end();
}


void checkDoubleReset() {
    // Read the reset flag and last reset time from EEPROM
    int resetFlag = EEPROM.read(RESET_FLAG_ADDR);
    unsigned long lastResetTime = EEPROM.read(RESET_TIME_ADDR) * 1000; // Convert seconds to milliseconds

    // Current time in milliseconds
    unsigned long currentTime = millis();

    if (resetFlag == 1 && (currentTime - lastResetTime) <= DOUBLE_RESET_INTERVAL) {
        // Double reset detected
        Serial.println("Double reset detected! Triggering factory reset...");
        handleempty();
    } else {
        // Set the reset flag and store the current time
        EEPROM.write(RESET_FLAG_ADDR, 1);
        EEPROM.write(RESET_TIME_ADDR, currentTime / 1000); // Store time in seconds to save EEPROM space
        EEPROM.commit();
    }
}
String readEEPROMString(int startAddr, int maxLength) {
    String result = "";
    for (int i = startAddr; i < startAddr + maxLength; i++) {
        char c = EEPROM.read(i);
        if (c == '\0') break; // End of string
        result += c;
    }
    return result;
}

int readEEPROMInt(int startAddr) {
    int value = 0;
    for (int i = 0; i < 4; i++) { // Read 4 bytes (int)
        value |= EEPROM.read(startAddr + i) << (i * 8);
    }
    return value;
}

String trimProtocol(String url) {
  const String protocol = "https://";
  if (url.startsWith(protocol)) {
    url = url.substring(protocol.length()); // Remove "https://"
  }
  return url;
}

String trimString(const String &str) {
    int start = 0;
    int end = str.length() - 1;

    // Trim leading spaces
    while (start <= end && isspace(str[start])) {
        start++;
    }

    // Trim trailing spaces
    while (end >= start && isspace(str[end])) {
        end--;
    }

    // Return the trimmed substring
    return str.substring(start, end + 1);
}

// Function to check credentials in EEPROM
bool checkCredentials() {
    // Helper function to read strings from EEPROM
    auto readEEPROMString = [](int startAddr, int maxLength) -> String {
        String result = "";
        for (int i = startAddr; i < startAddr + maxLength; i++) {
            char c = EEPROM.read(i);
            if (c == '\0') break; // End of string
            result += c;
        }
        return result;
    };

    // Helper function to read an integer from EEPROM
    auto readEEPROMInt = [](int startAddr) -> int {
        int value = 0;
        for (int i = 0; i < 4; i++) { // Read 4 bytes (int)
            value |= EEPROM.read(startAddr + i) << (i * 8);
        }
        return value;
    };

    // Read stored credentials
    String storedSSID = readEEPROMString(SSID_ADDR, 32);        // Wi-Fi SSID
    String storedPass = readEEPROMString(PASS_ADDR, 32);        // Wi-Fi Password
    String storedMQTTServer = readEEPROMString(MQTT_SERVER_ADDR, 50); // MQTT Server
    String storedMQTTUser = readEEPROMString(MQTT_USER_ADDR, 32);     // MQTT Username
    String storedMQTTPass = readEEPROMString(MQTT_PASS_ADDR, 32);     // MQTT Password
    int storedMQTTPort = readEEPROMInt(MQTT_PORT_ADDR);         // MQTT Port

    // Check if any credentials are missing
    if (storedSSID.length() == 0 || storedPass.length() == 0 ||
        storedMQTTServer.length() == 0 || storedMQTTUser.length() == 0 ||
        storedMQTTPass.length() == 0 || storedMQTTPort == 0) {
        return true;      // Missing credentials, setup required
    }

    return false;         // All credentials are present
}
void createap() {
    // Initialize Access Point
    WiFi.softAP(String("Automation").c_str());

    Serial.print("ESP Access Point IP: ");
    Serial.println(WiFi.softAPIP());
    // Start DNS server to redirect all domains to ESP
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    // Handle root page and captive portal requests
    server.on("/", handleCaptivePortal);
    server.on("/generate_204", handleCaptivePortal); // Android captive portal
    server.on("/fwlink", handleCaptivePortal);       // Windows captive portal
    server.on("/hotspot-detect.html", handleCaptivePortal); // Apple captive portal
    server.on("/save", handleSave);

    // Start web server
    server.begin();
    
}
// Helper function to write a string to EEPROM
void writeEEPROMString(int startAddr, const String& value, int maxLength) {
    for (int i = 0; i < maxLength; i++) {
        if (i < value.length()) {
            EEPROM.write(startAddr + i, value[i]);
        } else {
            EEPROM.write(startAddr + i, '\0'); // Null-terminate
        }
    }
}

// Helper function to write an integer to EEPROM
void writeEEPROMInt(int startAddr, int value) {
    for (int i = 0; i < 4; i++) {
        EEPROM.write(startAddr + i, (value >> (i * 8)) & 0xFF);
    }
}
// Handle saving credentials
void handleSave() {
    // Read the GET parameters from the request
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    String mqttServer = server.arg("mqtt_server");
    String mqttUser = server.arg("mqtt_user");
    String mqttPass = server.arg("mqtt_password");
    int mqttPort = server.arg("mqtt_port").toInt();
    String room_name = server.arg("room_name");
    String device_1 = server.arg("device_1");
    String device_2 = server.arg("device_2");
    String device_3 = server.arg("device_3");
    String device_4 = server.arg("device_4");

    // Save credentials to EEPROM
    writeEEPROMString(SSID_ADDR, ssid, 32);
    writeEEPROMString(PASS_ADDR, password, 32);
    writeEEPROMString(MQTT_SERVER_ADDR, mqttServer, 80);
    writeEEPROMString(MQTT_USER_ADDR, mqttUser, 32);
    writeEEPROMString(MQTT_PASS_ADDR, mqttPass, 32);
    writeEEPROMInt(MQTT_PORT_ADDR, mqttPort);
    writeEEPROMString(ROOM_NAME, room_name, 30);
    writeEEPROMString(RELAY_ONE, device_1, 30);
    writeEEPROMString(RELAY_TWO, device_2, 30);
    writeEEPROMString(RELAY_THREE, device_3, 30);
    writeEEPROMString(RELAY_FOUR, device_4, 30);
    EEPROM.commit();

    // Respond with confirmation
    server.send(200, "text/html", "<h2>Credentials Saved!</h2><p>Restarting the device.</p>");
    ESP.restart(); // Restart the ESP
    return; // Exit the function
}
// Handle empty credentials
void handleempty() {
    // Write empty strings to each address
    writeEEPROMString(SSID_ADDR, "", 32);
    writeEEPROMString(PASS_ADDR, "", 32);
    writeEEPROMString(MQTT_SERVER_ADDR, "", 80);
    writeEEPROMString(MQTT_USER_ADDR, "", 32);
    writeEEPROMString(MQTT_PASS_ADDR, "", 32);
    writeEEPROMInt(MQTT_PORT_ADDR, 0);
    writeEEPROMString(ROOM_NAME, "", 30);
    writeEEPROMString(RELAY_ONE, "", 30);
    writeEEPROMString(RELAY_TWO, "", 30);
    writeEEPROMString(RELAY_THREE, "", 30);
    writeEEPROMString(RELAY_FOUR, "", 30);
    EEPROM.put(10,0);
    EEPROM.put(20,0);
    EEPROM.put(30,0);
    EEPROM.put(40,0);
    EEPROM.put(50,0);
    EEPROM.commit();
    setupmode = true;
    ESP.restart(); // Restart the ESP
    return; // Exit the function
}
// Handle well-known URLs for captive portal detection
void handleCaptivePortal() {
    String html = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Setup Configuration</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #f9f9f9;
            margin: 0;
            padding: 20px;
        }

        .container {
            max-width: 600px;
            margin: auto;
            background-color: #fff;
            border-radius: 8px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
            padding: 20px;
            box-sizing: border-box;
        }

        .section {
            border: 1px solid #ccc;
            border-radius: 5px;
            padding: 15px;
            margin-bottom: 20px;
        }

        .section h3 {
            margin-top: 0;
            color: #333;
            font-size: 1.2em;
        }

        .form-row {
            display: flex;
            justify-content: space-between;
            gap: 10px;
            margin-bottom: 15px;
            flex-wrap: wrap;
        }

        .form-row input {
            flex: 1;
            min-width: 100px;
            padding: 10px;
            font-size: 14px;
            border: 1px solid #ccc;
            border-radius: 4px;
            box-sizing: border-box;
        }

        .form-row input:focus {
            border-color: #4CAF50;
            outline: none;
        }

        button {
            width: 100%;
            padding: 10px;
            font-size: 16px;
            border: none;
            border-radius: 4px;
            background-color: #4CAF50;
            color: #fff;
            cursor: pointer;
        }

        button:hover {
            background-color: #45a049;
        }

        @media (max-width: 600px) {
            .form-row {
                flex-direction: column;
            }

            .form-row input {
                width: 100%;
            }
        }
    </style>
</head>
<body>

<div class="container">
<form action="/save" method="GET">
    <h2>Home Automation Setup</h2>    
    <div class="section">
        <h3>Wi-Fi Credentials</h3>
        <div class="form-row">
            <input type="text" name="ssid" placeholder="Wi-Fi SSID" required>
            <input type="password" name="password" placeholder="Wi-Fi Password" required>
        </div>
    </div>

    <div class="section">
        <h3>MQTT Credentials</h3>
        <div class="form-row">
            <input type="text" name="mqtt_server" placeholder="MQTT Server" required>
            <input type="number" name="mqtt_port" placeholder="MQTT Port" required>
        </div>
        <div class="form-row">
            <input type="text" name="mqtt_user" placeholder="MQTT Username" required>
            <input type="password" name="mqtt_password" placeholder="MQTT Password" required>
        </div>
    </div>

    <div class="section">
        <h3>Room and Device Details</h3>
        <div class="form-row">
            <input type="text" name="room_name" placeholder="Room Name" required>
        </div>
        <div class="form-row">
            <input type="text" name="device_1" placeholder="Device 1 Name" required>
            <input type="text" name="device_2" placeholder="Device 2 Name" required>
        </div>
        <div class="form-row">
            <input type="text" name="device_3" placeholder="Device 3 Name" required>
            <input type="text" name="device_4" placeholder="Device 4 Name" required>
        </div>
    </div>

    <button type="submit">Save Configuration</button>
</form>
</div>

</body>
</html>

    )=====";
    server.send(200, "text/html", html);
}
// Save relay states to EEPROM
void saveRelayStates() {
  EEPROM.write(1, relayStates1);
  EEPROM.write(2, relayStates2);
  EEPROM.write(3, relayStates3);
  EEPROM.write(4, relayStates4);
  EEPROM.commit();
  play();
}
// Publish DHT11 data to MQTT
void publishDHTData() {
  if (!client.connected() && !isInSetMode){
    return;
  }

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  // Check if the readings are valid
  if (isnan(humidity) || isnan(temperature)) {
    return;
  }

  String payload = "[" + String(temperature) + "," + String(humidity) + "]";
  client.publish("home/dht/data", payload.c_str());
}

String roomname;
String relayName1;
String relayName2;
String relayName3;
String relayName4;

void publishAllRelayStates() {
  if (!client.connected() && !isInSetMode){
    return; // Skip if MQTT is not connected
  } 
  String payload = "{";
  payload += "\"room\": \"" + roomname + "\",";
  payload += "\"" + relayName1 + "\":\"" + (relayStates1 ? "on" : "off") + "\",";
  payload += "\"" + relayName2 + "\":\"" + (relayStates2 ? "on" : "off") + "\",";
  payload += "\"" + relayName3 + "\":\"" + (relayStates3 ? "on" : "off") + "\",";
  payload += "\"" + relayName4 + "\":\"" + (relayStates4 ? "on" : "off") + "\"";
  payload += "}";
  client.publish("home/relay/all/status", payload.c_str());
  publishDHTData();
}
// Handle switch toggling independently
unsigned long lastToggleTime1= 0;
unsigned long lastToggleTime2= 0;
unsigned long lastToggleTime3= 0;
unsigned long lastToggleTime4= 0;
void handleSwitches() {
  unsigned long currentTime = millis(); // Get the current time
  bool currentSwitchState1 = digitalRead(switchPin1) == LOW;
  bool currentSwitchState2 = digitalRead(switchPin2) == LOW;
  bool currentSwitchState3 = digitalRead(switchPin3) == LOW;
  bool currentSwitchState4 = digitalRead(switchPin4) == LOW;

  if (currentSwitchState1 != previousSwitchStates1 && (currentTime - lastToggleTime1 >= 500)) {
    relayStates1 = !relayStates1;
    digitalWrite(relayPin1, relayStates1 ? LOW : HIGH);
    lastToggleTime1 = currentTime;
    previousSwitchStates1 = currentSwitchState1;
    saveRelayStates();
    publishAllRelayStates();
  }
  if (currentSwitchState2 != previousSwitchStates2 && (currentTime - lastToggleTime2 >= 500)) {
    relayStates2 = !relayStates2;
    digitalWrite(relayPin2, relayStates2 ? LOW : HIGH);
    lastToggleTime2 = currentTime;
    previousSwitchStates2 = currentSwitchState2;
    saveRelayStates();
    publishAllRelayStates();
  }
  if (currentSwitchState3 != previousSwitchStates3 && (currentTime - lastToggleTime3 >= 500)) {
    relayStates3 = !relayStates3;
    digitalWrite(relayPin3, relayStates3 ? LOW : HIGH);
    lastToggleTime3 = currentTime;
    previousSwitchStates3 = currentSwitchState3;
    saveRelayStates();
    publishAllRelayStates();
  }
  if (currentSwitchState4 != previousSwitchStates4 && (currentTime - lastToggleTime4 >= 500)) {
    relayStates4 = !relayStates4;
    digitalWrite(relayPin4, relayStates4 ? LOW : HIGH);
    lastToggleTime4 = currentTime;
    previousSwitchStates4 = currentSwitchState4;
    saveRelayStates();
    publishAllRelayStates();
  }
  

}
void handleIRSignal() {
  if (isInSetMode) return; // Skip if in set mode

  if (IrReceiver.decode()) {
    uint32_t receivedCode = IrReceiver.decodedIRData.decodedRawData; // Get the raw IR data

    uint32_t irCodes1;
    uint32_t irCodes2;
    uint32_t irCodes3;
    uint32_t irCodes4;
    uint32_t irCodes5;

    // Retrieve stored IR codes from EEPROM
    EEPROM.get(10, irCodes1);
    EEPROM.get(20, irCodes2);
    EEPROM.get(30, irCodes3);
    EEPROM.get(40, irCodes4);
    EEPROM.get(50, irCodes5);

    // Check and handle the received IR code
    if (receivedCode == irCodes1) {
      relayStates1 = !relayStates1;
      digitalWrite(relayPin1, relayStates1 ? LOW : HIGH);
      saveRelayStates();
      publishAllRelayStates();
    } else if (receivedCode == irCodes2) {
      relayStates2 = !relayStates2;
      digitalWrite(relayPin2, relayStates2 ? LOW : HIGH);
      saveRelayStates();
      publishAllRelayStates();
    } else if (receivedCode == irCodes3) {
      relayStates3 = !relayStates3;
      digitalWrite(relayPin3, relayStates3 ? LOW : HIGH);
      saveRelayStates();
      publishAllRelayStates();
    } else if (receivedCode == irCodes4) {
      relayStates4 = !relayStates4;
      digitalWrite(relayPin4, relayStates4 ? LOW : HIGH);
      saveRelayStates();
      publishAllRelayStates();
    } else if (receivedCode == irCodes5) {
      relayStates5 = !relayStates5;

      // Toggle all relays together
      relayStates1 = relayStates5;
      relayStates2 = relayStates5;
      relayStates3 = relayStates5;
      relayStates4 = relayStates5;

      digitalWrite(relayPin1, relayStates5 ? LOW : HIGH);
      digitalWrite(relayPin2, relayStates5 ? LOW : HIGH);
      digitalWrite(relayPin3, relayStates5 ? LOW : HIGH);
      digitalWrite(relayPin4, relayStates5 ? LOW : HIGH);
      saveRelayStates();
      publishAllRelayStates();
    }

    // Resume the IR receiver for the next signal
    IrReceiver.resume();
  }
}

// Handle IR Set Mode
void handleIRSetMode() {
  if (!isInSetMode) return;

  static unsigned long lastBlinkTime = 0;
  static bool ledState = false;

  // Blink onboard LED
  if (millis() - lastBlinkTime >= 500) {
    lastBlinkTime = millis();
    ledState = !ledState;

    // Toggle the corresponding relay's state for blinking
    switch (currentSetRelay) {
      case 1:
        digitalWrite(relayPin1, ledState ? LOW : HIGH);
        break;
      case 2:
        digitalWrite(relayPin2, ledState ? LOW : HIGH);
        break;
      case 3:
        digitalWrite(relayPin3, ledState ? LOW : HIGH);
        break;
      case 4:
        digitalWrite(relayPin4, ledState ? LOW : HIGH);
        break;
      case 5:
        digitalWrite(relayPin1, ledState ? LOW : HIGH);
        digitalWrite(relayPin2, ledState ? LOW : HIGH);
        digitalWrite(relayPin3, ledState ? LOW : HIGH);
        digitalWrite(relayPin4, ledState ? LOW : HIGH);
        break;
    }
  }

  // Check for IR signal reception
  if (IrReceiver.decode()) {
    uint32_t receivedCode = IrReceiver.decodedIRData.decodedRawData; // Get the raw IR data

    // Store the IR code in EEPROM based on the current relay being set
    switch (currentSetRelay) {
      case 1:
        EEPROM.put(10, receivedCode);
        EEPROM.commit();
        break;
      case 2:
        EEPROM.put(20, receivedCode);
        EEPROM.commit();
        break;
      case 3:
        EEPROM.put(30, receivedCode);
        EEPROM.commit();
        break;
      case 4:
        EEPROM.put(40, receivedCode);
        EEPROM.commit();
        break;
      case 5:
        EEPROM.put(50, receivedCode);
        EEPROM.commit();
        break;
    }

    // Exit set mode and reset the relay states
    isInSetMode = false;
    currentSetRelay = -1;

    // Restore the relay states if not setting a relay
    if (currentSetRelay != 1) {
      digitalWrite(relayPin1, relayStates1 ? LOW : HIGH);
      digitalWrite(relayPin2, relayStates2 ? LOW : HIGH);
      digitalWrite(relayPin3, relayStates3 ? LOW : HIGH);
      digitalWrite(relayPin4, relayStates4 ? LOW : HIGH);

      publishAllRelayStates();
    }

    // Resume the IR receiver for the next signal
    IrReceiver.resume();
    play();
  }
}




// Callback function to handle incoming messages
void messageReceived(String &topic, String &payload) {
  if (topic == "home/relay/all/get" && payload == "status") {
    publishAllRelayStates();
  }
  String mqttUser = trimString(readEEPROMString(MQTT_USER_ADDR, 32));
  if (topic == "home/ir/all/set" && payload == mqttUser.c_str()) {
    isInSetMode = true;
    currentSetRelay = 5;
    return;
  }
  if (topic == "home/ir/1/set" && payload == mqttUser.c_str()) {
      isInSetMode = true;
      currentSetRelay = 1;      
      return;
  }
  if (topic == "home/ir/2/set" && payload == mqttUser.c_str()) {
      isInSetMode = true;
      currentSetRelay = 2;      
      return;
  }
  if (topic == "home/ir/3/set" && payload == mqttUser.c_str()) {
      isInSetMode = true;
      currentSetRelay = 3;      
      return;
  }
  if (topic == "home/ir/4/set" && payload == mqttUser.c_str()) {
      isInSetMode = true;
      currentSetRelay = 4;  
      return;
  }
  if (topic == "home/ir/all/unset" && payload == mqttUser.c_str()) {
      EEPROM.put(50,0);
      EEPROM.commit();
      publishAllRelayStates();
      return;
  }
  if (topic == "home/ir/1/unset" && payload == mqttUser.c_str()) {
      EEPROM.put(10,0);
      EEPROM.commit();
      publishAllRelayStates();
      return;
  }
  if (topic == "home/ir/2/unset" && payload == mqttUser.c_str()) {
      EEPROM.put(20,0);
      EEPROM.commit();
      publishAllRelayStates();
      return;
  }
  if (topic == "home/ir/3/unset" && payload == mqttUser.c_str()) {
      EEPROM.put(30,0);
      EEPROM.commit();
      publishAllRelayStates();
      return;
  }
  if (topic == "home/ir/4/unset" && payload == mqttUser.c_str()) {
      EEPROM.put(40,0);
      EEPROM.commit();
      publishAllRelayStates();
      return;
  }

  if (topic == "home/relay/all/set") {
    bool allOn = (payload == "on");
    relayStates1 = allOn;
    digitalWrite(relayPin1, allOn ? LOW : HIGH);
    relayStates2 = allOn;
    digitalWrite(relayPin2, allOn ? LOW : HIGH);
    relayStates3 = allOn;
    digitalWrite(relayPin3, allOn ? LOW : HIGH);
    relayStates4 = allOn;
    digitalWrite(relayPin4, allOn ? LOW : HIGH);
    saveRelayStates();
    publishAllRelayStates();
  }
  if (topic == "home/relay/1/set") {
      relayStates1 = (payload == "on");
      digitalWrite(relayPin1, relayStates1 ? LOW : HIGH);
      saveRelayStates();
      publishAllRelayStates();      
  }
  if (topic == "home/relay/2/set") {
      relayStates2 = (payload == "on");
      digitalWrite(relayPin2, relayStates2 ? LOW : HIGH);
      saveRelayStates();
      publishAllRelayStates();
  }
  if (topic == "home/relay/3/set") {
      relayStates3 = (payload == "on");
      digitalWrite(relayPin3, relayStates3 ? LOW : HIGH);
      saveRelayStates();
      publishAllRelayStates();
  }
  if (topic == "home/relay/4/set") {
      relayStates4 = (payload == "on");
      digitalWrite(relayPin4, relayStates4 ? LOW : HIGH);
      saveRelayStates();
      publishAllRelayStates();
  }
  if (topic == "home/automation/reset" && payload == mqttUser.c_str()) {
    ESP.restart();
    return;
  }if (topic == "home/factory/reset" && payload == mqttUser.c_str()) {
    handleempty();
  }
}

void connectToWiFi(){


  WiFi.begin(readEEPROMString(SSID_ADDR, 32).c_str(), readEEPROMString(PASS_ADDR, 32).c_str()); 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected");

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true; // Mark Wi-Fi as connected
    Serial.println("\nConnected to Wi-Fi!");
    connectToMQTT();
  } else {
    Serial.println("\nConnection failed. Will retry...");
  }
}
void connectToMQTT(){
  if (WiFi.status() == WL_CONNECTED && !mqttsubscribe){
    // Configure WiFiClientSecure
      net.setInsecure();
   
      // Initialize MQTT client
      String mqttServer = trimString(trimProtocol(readEEPROMString(MQTT_SERVER_ADDR, 80)));
      client.begin(mqttServer.c_str(), readEEPROMInt(MQTT_PORT_ADDR), net);
      client.onMessage(messageReceived);

      // Connect to MQTT
      Serial.println("Connecting to MQTT...");
      String mqttUser = trimString(readEEPROMString(MQTT_USER_ADDR, 32));
      String mqttPass = trimString(readEEPROMString(MQTT_PASS_ADDR, 80));
      while (!client.connect("ESP8266Client", mqttUser.c_str(), mqttPass.c_str())) {
        Serial.print(".");
        delay(1000);
      }


      // // Subscribe to MQTT
      client.subscribe("home/relay/all/get");
      
      client.subscribe("home/relay/1/set");
      client.subscribe("home/relay/2/set");
      client.subscribe("home/relay/3/set");
      client.subscribe("home/relay/4/set");
      client.subscribe("home/relay/all/set");

      client.subscribe("home/ir/1/set");
      client.subscribe("home/ir/2/set");
      client.subscribe("home/ir/3/set");
      client.subscribe("home/ir/4/set");
      client.subscribe("home/ir/all/set");

      client.subscribe("home/ir/1/unset");
      client.subscribe("home/ir/2/unset");
      client.subscribe("home/ir/3/unset");
      client.subscribe("home/ir/4/unset");
      client.subscribe("home/ir/all/unset");      

      client.subscribe("home/automation/reset");
      client.subscribe("home/factory/reset");

      roomname = readEEPROMString(ROOM_NAME, 30);
      relayName1 = readEEPROMString(RELAY_ONE, 30);
      relayName2 = readEEPROMString(RELAY_TWO, 30);
      relayName3 = readEEPROMString(RELAY_THREE, 30);
      relayName4 = readEEPROMString(RELAY_FOUR, 30);

      mqttsubscribe = true;

      // Publish start status
      publishAllRelayStates();


      Serial.println("Connected to MQTT!");
      play();
  }
  return; 

}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(2048);

  Serial.println("Checking for double reset...");
  checkDoubleReset(); // Check if this is a double reset

  // Clear the reset flag after a short delay to avoid false triggers
  delay(1000); // Wait to allow for the next reset (if any)
  EEPROM.write(RESET_FLAG_ADDR, 0); // Clear the reset flag
  EEPROM.commit();

  // Rest of the setup code...
  Serial.println("Normal boot. Proceeding with setup...");


  if (checkCredentials()) {
      setupmode = true;
  } else {
      setupmode = false;
  }
  if(setupmode){
    createap();
  }else{
    pinMode(relayPin1, OUTPUT);
    pinMode(relayPin2, OUTPUT);
    pinMode(relayPin3, OUTPUT);
    pinMode(relayPin4, OUTPUT);
    relayStates1 = EEPROM.read(1);
    relayStates2 = EEPROM.read(2);
    relayStates3 = EEPROM.read(3);
    relayStates4 = EEPROM.read(4);
    digitalWrite(relayPin1, relayStates1 ? LOW : HIGH);
    digitalWrite(relayPin2, relayStates2 ? LOW : HIGH);
    digitalWrite(relayPin3, relayStates3 ? LOW : HIGH);
    digitalWrite(relayPin4, relayStates4 ? LOW : HIGH);

    // Initialize switches
    pinMode(switchPin1, INPUT_PULLUP);
    previousSwitchStates1 = digitalRead(switchPin1) == LOW;
    pinMode(switchPin2, INPUT_PULLUP);
    previousSwitchStates2 = digitalRead(switchPin2) == LOW;
    pinMode(switchPin3, INPUT_PULLUP);
    previousSwitchStates3 = digitalRead(switchPin3) == LOW;
    pinMode(switchPin4, INPUT_PULLUP);
    previousSwitchStates4 = digitalRead(switchPin4) == LOW;


    // Initialize IR receiver
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
    play();
  }
  
}

void loop() {
  if(setupmode){
    dnsServer.processNextRequest(); // Handle DNS requests
    server.handleClient();          // Handle HTTP requests
  }else{
    handleSwitches();
    handleIRSignal();
    handleIRSetMode();

    if (!wifiConnected) {
      unsigned long currentTime = millis();
      if (currentTime - lastRetryTime >= retryInterval) {
        connectToWiFi();
        lastRetryTime = currentTime; 
      }
    } else {
      
    }
    // Maintain MQTT connection
    client.loop();
    // Periodically publish messages
    static unsigned long lastPublish = 0;
    if (millis() - lastPublish > dhtpublishinterval) {
      lastPublish = millis();
      publishDHTData();
    }


    if (millis() - previousrestarMillis >= resetinmls) {
      ESP.restart();
    }
    if(wifiConnected && !updatecheck){
      checkForOTAUpdate();
    }
  }
}
