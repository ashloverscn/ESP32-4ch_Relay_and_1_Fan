#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>

Preferences preferences;

// --- Default Fallback Credentials (Pre-filled on first boot) ---
const char* DEFAULT_SSID = "ash_jio_2G";
const char* DEFAULT_PASSWORD = "ThundersnO0w@,,#";

const char* DEFAULT_MQTT_SERVER = "i11b0026.ala.eu-central-1.emqxsl.com";
const int   DEFAULT_MQTT_PORT = 8883;
const char* DEFAULT_MQTT_USER = "admin";
const char* DEFAULT_MQTT_PASS = "admin1234S";
const char* DEFAULT_MQTT_TOPIC_CMD = "home/esp32/command";
const char* DEFAULT_MQTT_TOPIC_STATE = "home/esp32/state";

// --- Runtime Configuration Variables ---
String stored_ssid;
String stored_password;
String mqtt_server;
int mqtt_port;
String mqtt_user;
String mqtt_pass;
String mqtt_topic_cmd;
String mqtt_topic_state;

// --- Controller Hardware Pin Definitions (Fixed GPIO 5 Conflict) ---
const int RelayPin1 = 21;       // Moved from GPIO 5 to avoid boot loop
const int RelayPin2 = 18;
const int RelayPin3 = 19;
const int RelayPin4 = 23;
const int CONFIG_BUTTON_PIN = 0;// Boot button for AP mode fallback

// --- Controller State ---
bool relayStates[4] = {false, false, false, false};
int dimmerValue = 0; // 0 to 9

// --- Modern Web Server, WebSocket & MQTT ---
WebServer server(80);
WebSocketsServer webSocket(81);
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// --- Serial Buffering State (50ms Silence Timeout) ---
String serialBuffer = "";
unsigned long lastCharTime = 0;
const unsigned long SERIAL_TIMEOUT = 50;

// --- Wi-Fi Reconnection & AP Fallback State ---
unsigned long lastWiFiCheckTime = 0;
const unsigned long WIFI_RETRY_INTERVAL = 10000; 
unsigned long wifiDisconnectedStartTime = 0;       
const unsigned long WIFI_AP_FALLBACK_TIMEOUT = 600000; // 10 minutes

// --- GPIO 0 Long Press & AP Mode State ---
unsigned long buttonPressStartTime = 0;
bool buttonStateLast = HIGH;
const unsigned long LONG_PRESS_DURATION = 3000; 
bool apModeActive = false;

// Forward declarations
void loadConfig();
void saveConfig(String ssid, String pass, String srv, int prt, String usr, String pwd, String tCmd, String tState);
void broadcastState();
void logMessage(const String& msg);
void handleCommand(const String& cmd);
void setRelayState(int relay, bool turnOn);
void processBufferedSerial(const String& data);
void reconnectMQTT();
void callbackMQTT(char* topic, byte* payload, unsigned int length);
void checkWiFiConnection();
void checkButton();
void enterAPMode();

// --- HTML Template (Dashboard) ---
const char HTML_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Home Automation Web Controller</title>
    <style>
        body { font-family: Arial, sans-serif; background-color: #f4f6f8; margin: 0; padding: 20px; display: flex; justify-content: center; }
        .container { width: 100%; max-width: 650px; background: #ffffff; border-radius: 8px; padding: 20px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); position: relative; }
        .config-link { position: absolute; top: 20px; right: 20px; text-decoration: none; background-color: #6c757d; color: white; padding: 8px 12px; font-size: 13px; border-radius: 4px; font-weight: bold; }
        .config-link:hover { background-color: #5a6268; }
        h2 { text-align: center; margin-top: 0; color: #333; }
        .card { border: 1px solid #ddd; border-radius: 6px; padding: 15px; margin-bottom: 15px; background: #fafafa; }
        .card-title { font-weight: bold; margin-bottom: 10px; color: #555; font-size: 14px; text-transform: uppercase; }
        .row { display: flex; gap: 10px; align-items: center; flex-wrap: wrap; }
        button { padding: 10px 15px; font-size: 14px; border-radius: 4px; border: none; cursor: pointer; background-color: #007bff; color: white; transition: 0.2s; }
        button:hover { background-color: #0056b3; }
        .relay-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px; }
        .relay-btn { height: 70px; font-weight: bold; background-color: #e0e0e0; color: #000; border-radius: 6px; }
        .relay-btn.on { background-color: #4CAF50; color: #fff; }
        #console { width: 100%; height: 150px; background-color: #1e1e1e; color: #00ff00; font-family: monospace; font-size: 12px; padding: 10px; box-sizing: border-box; overflow-y: scroll; border-radius: 4px; white-space: pre-wrap; }
        .flex-fill { flex: 1; }
        #connectionStatus { font-weight: bold; padding: 5px 10px; border-radius: 4px; display: inline-block; }
        .status-disconnected { background: #ffebee; color: #c62828; }
        .status-connected { background: #e8f5e9; color: #2e7d32; }
    </style>
</head>
<body>
<div class="container">
    <a href="/config" class="config-link">⚙️ Config</a>
    <h2>ESP32 Home Automation</h2>
    <div class="card">
        <div class="card-title">ESP32 Status</div>
        <div id="connectionStatus" class="status-disconnected">Status: Connecting...</div>
    </div>
    <div class="card">
        <div class="card-title">Relay Controls</div>
        <div class="relay-grid">
            <button id="relay-1" class="relay-btn" onclick="toggleRelay(1)">Relay 1<br>OFF</button>
            <button id="relay-2" class="relay-btn" onclick="toggleRelay(2)">Relay 2<br>OFF</button>
            <button id="relay-3" class="relay-btn" onclick="toggleRelay(3)">Relay 3<br>OFF</button>
            <button id="relay-4" class="relay-btn" onclick="toggleRelay(4)">Relay 4<br>OFF</button>
        </div>
    </div>
    <div class="card">
        <div class="card-title">Dimmer Control (0-9)</div>
        <div class="row">
            <button onclick="sendCommand('DIMMER 0')">MIN (0)</button>
            <button onclick="sendCommand('DIMM_DN')">Step -</button>
            <input type="range" id="dimmerSlider" class="flex-fill" min="0" max="9" value="0" onchange="onSliderChange(this.value)">
            <button onclick="sendCommand('DIMM_UP')">Step +</button>
            <button onclick="sendCommand('DIMMER 9')">MAX (9)</button>
        </div>
    </div>
    <div class="card">
        <div class="card-title">Global Commands & Status</div>
        <div class="row">
            <button class="flex-fill" onclick="sendCommand('ALL_ON')">ALL ON</button>
            <button class="flex-fill" onclick="sendCommand('ALL_OFF')">ALL OFF</button>
            <button class="flex-fill" onclick="sendCommand('STATUS')">Get Status</button>
        </div>
    </div>
    <div class="card">
        <div class="card-title">Serial Console / Feedback</div>
        <div id="console"></div>
    </div>
</div>
<script>
    let ws;
    let relayStates = [false, false, false, false];
    let reconnectAttempts = 0;
    function initWebSocket() {
        ws = new WebSocket('ws://' + window.location.hostname + ':81/');
        ws.onopen = () => { reconnectAttempts = 0; updateConnectionBanner(true); appendConsole("Connected to ESP32 WebSocket server."); sendCommand('STATUS'); };
        ws.onclose = () => {
            updateConnectionBanner(false);
            let timeout = Math.min(1000 * Math.pow(1.5, reconnectAttempts), 10000);
            appendConsole(`Disconnected. Reconnecting in ${Math.round(timeout/1000)}s...`);
            setTimeout(initWebSocket, timeout);
            reconnectAttempts++;
        };
        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.type === 'console_log') appendConsole(data.message);
                else if (data.type === 'state_update') {
                    if (data.relays) data.relays.forEach((state, i) => updateRelayUI(i + 1, state));
                    if (data.dimmer !== undefined) document.getElementById('dimmerSlider').value = data.dimmer;
                }
            } catch (e) { appendConsole(event.data); }
        };
    }
    function updateConnectionBanner(connected) {
        const banner = document.getElementById('connectionStatus');
        banner.innerText = connected ? "Status: Connected (ESP32 Direct)" : "Status: Disconnected";
        banner.className = connected ? "status-connected" : "status-disconnected";
    }
    function toggleRelay(relayNum) { sendCommand(`POWER${relayNum} ${relayStates[relayNum - 1] ? 'OFF' : 'ON'}`); }
    function onSliderChange(val) { sendCommand(`DIMMER ${val}`); }
    function sendCommand(cmd) { if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify({command: cmd})); }
    function updateRelayUI(relayNum, isOn) {
        relayStates[relayNum - 1] = isOn;
        const btn = document.getElementById(`relay-${relayNum}`);
        btn.innerText = `Relay ${relayNum}\n${isOn ? 'ON' : 'OFF'}`;
        if (isOn) btn.classList.add('on'); else btn.classList.remove('on');
    }
    function appendConsole(msg) {
        const consoleEl = document.getElementById('console');
        consoleEl.innerHTML += msg + '\n';
        consoleEl.scrollTop = consoleEl.scrollHeight;
    }
    window.onload = initWebSocket;
</script>
</body>
</html>
)rawliteral";

// --- Dedicated Wi-Fi & MQTT Configuration Page HTML ---
const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Device Configuration</title>
    <style>
        body { font-family: Arial, sans-serif; background-color: #f4f6f8; margin: 0; padding: 20px; display: flex; justify-content: center; }
        .container { width: 100%; max-width: 500px; background: #ffffff; border-radius: 8px; padding: 20px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
        h2 { text-align: center; margin-top: 0; color: #333; }
        .back-link { display: inline-block; margin-bottom: 15px; text-decoration: none; color: #007bff; font-weight: bold; font-size: 14px; }
        label { font-weight: bold; font-size: 13px; color: #555; display: block; margin-top: 10px; }
        input[type="text"], input[type="password"], input[type="number"] { padding: 8px; border: 1px solid #ccc; border-radius: 4px; width: 100%; box-sizing: border-box; margin-top: 5px; margin-bottom: 5px; }
        hr { border: 0; border-top: 1px solid #eee; margin: 20px 0; }
        button { width: 100%; padding: 12px; font-size: 15px; border-radius: 4px; border: none; cursor: pointer; background-color: #28a745; color: white; font-weight: bold; margin-top: 15px; }
        button:hover { background-color: #218838; }
    </style>
</head>
<body>
<div class="container">
    <a href="/" class="back-link">&larr; Back to Dashboard</a>
    <h2>Device Configuration</h2>
    <form action="/save-settings" method="POST">
        <h3>Wi-Fi Settings</h3>
        <label>Wi-Fi SSID:</label><input type="text" name="wifi_ssid" id="cfg_ssid">
        <label>Wi-Fi Password:</label><input type="password" name="wifi_pass" id="cfg_pass_wifi">
        <hr>
        <h3>MQTT Settings</h3>
        <label>MQTT Server:</label><input type="text" name="mqtt_server" id="cfg_server">
        <label>MQTT Port:</label><input type="number" name="mqtt_port" id="cfg_port">
        <label>MQTT Username:</label><input type="text" name="mqtt_user" id="cfg_user">
        <label>MQTT Password:</label><input type="password" name="mqtt_pass" id="cfg_pass">
        <label>Command Topic:</label><input type="text" name="mqtt_topic_cmd" id="cfg_tcmd">
        <label>State Topic:</label><input type="text" name="mqtt_topic_state" id="cfg_tstate">
        <button type="submit">Save & Reboot</button>
    </form>
</div>
<script>
    fetch('/get-config').then(res => res.json()).then(data => {
        document.getElementById('cfg_ssid').value = data.ssid || '';
        document.getElementById('cfg_pass_wifi').value = data.pass_wifi || '';
        document.getElementById('cfg_server').value = data.server;
        document.getElementById('cfg_port').value = data.port;
        document.getElementById('cfg_user').value = data.user;
        document.getElementById('cfg_pass').value = data.pass;
        document.getElementById('cfg_tcmd').value = data.tcmd;
        document.getElementById('cfg_tstate').value = data.tstate;
    });
</script>
</body>
</html>
)rawliteral";

// --- Relay Control & State Persistence ---
void setRelayState(int relay, bool turnOn) {
    int pin = 0;
    String prefKey = "";
    switch (relay) {
        case 1: pin = RelayPin1; prefKey = "relay1"; break;
        case 2: pin = RelayPin2; prefKey = "relay2"; break;
        case 3: pin = RelayPin3; prefKey = "relay3"; break;
        case 4: pin = RelayPin4; prefKey = "relay4"; break;
        default: return;
    }
    relayStates[relay - 1] = turnOn;
    digitalWrite(pin, turnOn ? LOW : HIGH); // Active LOW relay convention support

    preferences.begin("config", false);
    preferences.putUChar(prefKey.c_str(), turnOn ? 1 : 0);
    preferences.end();
}

// --- Load and Save Configurations via Preferences ---
void loadConfig() {
    preferences.begin("config", false);
    stored_ssid = preferences.getString("ssid", DEFAULT_SSID);
    stored_password = preferences.getString("password", DEFAULT_PASSWORD);
    mqtt_server = preferences.getString("server", DEFAULT_MQTT_SERVER);
    mqtt_port = preferences.getInt("port", DEFAULT_MQTT_PORT);
    mqtt_user = preferences.getString("user", DEFAULT_MQTT_USER);
    mqtt_pass = preferences.getString("pass", DEFAULT_MQTT_PASS);
    mqtt_topic_cmd = preferences.getString("tcmd", DEFAULT_MQTT_TOPIC_CMD);
    mqtt_topic_state = preferences.getString("tstate", DEFAULT_MQTT_TOPIC_STATE);

    uint8_t r1 = preferences.getUChar("relay1", 0);
    uint8_t r2 = preferences.getUChar("relay2", 0);
    uint8_t r3 = preferences.getUChar("relay3", 0);
    uint8_t r4 = preferences.getUChar("relay4", 0);
    dimmerValue = preferences.getUChar("dimmer", 0);
    preferences.end();

    setRelayState(1, r1 == 1);
    setRelayState(2, r2 == 1);
    setRelayState(3, r3 == 1);
    setRelayState(4, r4 == 1);
}

void saveConfig(String ssid, String pass, String srv, int prt, String usr, String pwd, String tCmd, String tState) {
    preferences.begin("config", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", pass);
    preferences.putString("server", srv);
    preferences.putInt("port", prt);
    preferences.putString("user", usr);
    preferences.putString("pass", pwd);
    preferences.putString("tcmd", tCmd);
    preferences.putString("tstate", tState);
    preferences.end();
}

// --- Verbose Console Logger ---
void logMessage(const String& msg) {
    Serial.println(msg);
    JsonDocument doc;
    doc["type"] = "console_log";
    doc["message"] = msg;
    String output;
    serializeJson(doc, output);
    webSocket.broadcastTXT(output);
}

// --- Continuous Non-Blocking Wi-Fi Reconnection & 10-Min AP Fallback ---
void checkWiFiConnection() {
    if (!apModeActive) {
        if (WiFi.status() == WL_CONNECTED) {
            wifiDisconnectedStartTime = 0; 
        } else {
            if (wifiDisconnectedStartTime == 0) {
                wifiDisconnectedStartTime = millis();
                logMessage(">> WiFi lost. Starting 10-minute AP fallback timer...");
            } 
            else if (millis() - wifiDisconnectedStartTime >= WIFI_AP_FALLBACK_TIMEOUT) {
                logMessage(">> WiFi reconnection timed out (10 mins exceeded). Entering AP Mode automatically...");
                enterAPMode();
                return;
            }

            unsigned long currentMillis = millis();
            if (currentMillis - lastWiFiCheckTime >= WIFI_RETRY_INTERVAL) {
                lastWiFiCheckTime = currentMillis;
                logMessage(">> WiFi Disconnected. Attempting reconnection...");
                WiFi.disconnect();
                WiFi.begin(stored_ssid.c_str(), stored_password.c_str());
            }
        }
    }
}

// --- GPIO 0 Long Press Detection ---
void checkButton() {
    int currentState = digitalRead(CONFIG_BUTTON_PIN);
    if (currentState == LOW && buttonStateLast == HIGH) {
        buttonPressStartTime = millis();
        buttonStateLast = LOW;
    } 
    else if (currentState == LOW && buttonStateLast == LOW) {
        if (!apModeActive && (millis() - buttonPressStartTime >= LONG_PRESS_DURATION)) {
            logMessage(">> Long press detected on GPIO 0! Entering AP Configuration Mode...");
            enterAPMode();
        }
    } 
    else if (currentState == HIGH && buttonStateLast == LOW) {
        buttonStateLast = HIGH;
    }
}

// --- Enter Access Point Mode for Reconfiguration ---
void enterAPMode() {
    apModeActive = true;
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    
    bool apStarted = WiFi.softAP("ESP32-Setup-AP", "12345678");
    if (apStarted) {
        logMessage(">> AP Mode Started Successfully! SSID: ESP32-Setup-AP | IP: http://192.168.4.1");
    }

    webSocket.close();
    server.stop();

    server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", CONFIG_PAGE); });
    server.on("/get-config", HTTP_GET, []() {
        JsonDocument doc;
        doc["ssid"] = stored_ssid; doc["pass_wifi"] = stored_password;
        doc["server"] = mqtt_server; doc["port"] = mqtt_port;
        doc["user"] = mqtt_user; doc["pass"] = mqtt_pass;
        doc["tcmd"] = mqtt_topic_cmd; doc["tstate"] = mqtt_topic_state;
        String response; serializeJson(doc, response);
        server.send(200, "application/json", response);
    });
    server.on("/save-settings", HTTP_POST, []() {
        String s_ssid = server.hasArg("wifi_ssid") ? server.arg("wifi_ssid") : DEFAULT_SSID;
        String s_pass = server.hasArg("wifi_pass") ? server.arg("wifi_pass") : DEFAULT_PASSWORD;
        String srv = server.hasArg("mqtt_server") ? server.arg("mqtt_server") : DEFAULT_MQTT_SERVER;
        int prt = server.hasArg("mqtt_port") ? server.arg("mqtt_port").toInt() : DEFAULT_MQTT_PORT;
        String usr = server.hasArg("mqtt_user") ? server.arg("mqtt_user") : DEFAULT_MQTT_USER;
        String pwd = server.hasArg("mqtt_pass") ? server.arg("mqtt_pass") : DEFAULT_MQTT_PASS;
        String tCmd = server.hasArg("mqtt_topic_cmd") ? server.arg("mqtt_topic_cmd") : DEFAULT_MQTT_TOPIC_CMD;
        String tState = server.hasArg("mqtt_topic_state") ? server.arg("mqtt_topic_state") : DEFAULT_MQTT_TOPIC_STATE;

        saveConfig(s_ssid, s_pass, srv, prt, usr, pwd, tCmd, tState);
        server.send(200, "text/html", "<h3>Settings Saved! Restarting ESP32...</h3><script>setTimeout(()=>{window.location='/'}, 4000);</script>");
        delay(1000);
        ESP.restart();
    });
    server.begin();
}

// --- WebSocket Event Callback ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_CONNECTED) broadcastState();
    else if (type == WStype_TEXT) {
        JsonDocument doc;
        if (!deserializeJson(doc, payload, length) && doc.containsKey("command")) {
            handleCommand(doc["command"].as<String>());
        }
    }
}

// --- MQTT Callback for Inbound Commands ---
void callbackMQTT(char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
    message.trim();
    
    logMessage("<< [MQTT Received]: " + message);
    if (String(topic) == mqtt_topic_cmd) {
        JsonDocument doc;
        if (!deserializeJson(doc, message) && doc.containsKey("command")) {
            handleCommand(doc["command"].as<String>());
        } else {
            handleCommand(message);
        }
    }
}

void reconnectMQTT() {
    if (!apModeActive && !mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
        static unsigned long lastMqttRetry = 0;
        if (millis() - lastMqttRetry > 10000) { 
            lastMqttRetry = millis();
            String clientId = "ESP32Gateway-" + String((uint32_t)ESP.getEfuseMac(), HEX);
            if (mqttClient.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_pass.c_str())) {
                mqttClient.subscribe(mqtt_topic_cmd.c_str());
            }
        }
    }
}

// --- Handle Commands ---
void handleCommand(const String& cmd) {
    String trimmedCmd = cmd;
    trimmedCmd.trim();

    JsonDocument cmdDoc;
    cmdDoc["command"] = trimmedCmd;
    String pubPayload;
    serializeJson(cmdDoc, pubPayload);
    mqttClient.publish(mqtt_topic_cmd.c_str(), pubPayload.c_str());

    for (int i = 1; i <= 4; i++) {
        if (trimmedCmd == "POWER" + String(i) + " ON") {
            setRelayState(i, true);
            broadcastState();
            return;
        } else if (trimmedCmd == "POWER" + String(i) + " OFF") {
            setRelayState(i, false);
            broadcastState();
            return;
        }
    }

    if (trimmedCmd.startsWith("DIMMER ")) {
        dimmerValue = trimmedCmd.substring(7).toInt();
        preferences.begin("config", false);
        preferences.putUChar("dimmer", dimmerValue);
        preferences.end();
    } else if (trimmedCmd == "DIMM_UP") {
        if (dimmerValue < 9) dimmerValue++;
    } else if (trimmedCmd == "DIMM_DN") {
        if (dimmerValue > 0) dimmerValue--;
    } else if (trimmedCmd == "ALL_ON") {
        for (int i = 1; i <= 4; i++) setRelayState(i, true);
        dimmerValue = 9;
    } else if (trimmedCmd == "ALL_OFF") {
        for (int i = 1; i <= 4; i++) setRelayState(i, false);
        dimmerValue = 0;
    }

    broadcastState();
}

// --- Serial Batch Processing ---
void processBufferedSerial(const String& data) {
    String bufferCopy = data;
    bufferCopy.trim();
    if (bufferCopy.length() == 0) return;
    int startIdx = 0;
    while (startIdx < (int)bufferCopy.length()) {
        int nextNewline = bufferCopy.indexOf('\n', startIdx);
        if (nextNewline == -1) nextNewline = bufferCopy.length();
        String line = bufferCopy.substring(startIdx, nextNewline);
        line.trim();
        if (line.length() > 0) handleCommand(line);
        startIdx = nextNewline + 1;
    }
}

// --- Broadcast Helpers ---
void broadcastState() {
    JsonDocument doc;
    doc["type"] = "state_update";
    JsonArray relays = doc["relays"].to<JsonArray>();
    for (int i = 0; i < 4; i++) relays.add(relayStates[i]);
    doc["dimmer"] = dimmerValue;

    String output;
    serializeJson(doc, output);
    webSocket.broadcastTXT(output);

    if (mqttClient.connected()) {
        mqttClient.publish(mqtt_topic_state.c_str(), output.c_str());
    }
}

// --- Setup & Loop ---
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
    pinMode(RelayPin1, OUTPUT);
    pinMode(RelayPin2, OUTPUT);
    pinMode(RelayPin3, OUTPUT);
    pinMode(RelayPin4, OUTPUT);

    loadConfig();

    WiFi.begin(stored_ssid.c_str(), stored_password.c_str());
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
        delay(500);
    }

    espClient.setInsecure();
    mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
    mqttClient.setCallback(callbackMQTT);
    mqttClient.setBufferSize(512);

    server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", HTML_TEMPLATE); });
    server.on("/config", HTTP_GET, []() { server.send_P(200, "text/html", CONFIG_PAGE); });
    server.on("/get-config", HTTP_GET, []() {
        JsonDocument doc;
        doc["ssid"] = stored_ssid; doc["pass_wifi"] = stored_password;
        doc["server"] = mqtt_server; doc["port"] = mqtt_port;
        doc["user"] = mqtt_user; doc["pass"] = mqtt_pass;
        doc["tcmd"] = mqtt_topic_cmd; doc["tstate"] = mqtt_topic_state;
        String response; serializeJson(doc, response);
        server.send(200, "application/json", response);
    });
    server.on("/save-settings", HTTP_POST, []() {
        String s_ssid = server.hasArg("wifi_ssid") ? server.arg("wifi_ssid") : DEFAULT_SSID;
        String s_pass = server.hasArg("wifi_pass") ? server.arg("wifi_pass") : DEFAULT_PASSWORD;
        String srv = server.hasArg("mqtt_server") ? server.arg("mqtt_server") : DEFAULT_MQTT_SERVER;
        int prt = server.hasArg("mqtt_port") ? server.arg("mqtt_port").toInt() : DEFAULT_MQTT_PORT;
        String usr = server.hasArg("mqtt_user") ? server.arg("mqtt_user") : DEFAULT_MQTT_USER;
        String pwd = server.hasArg("mqtt_pass") ? server.arg("mqtt_pass") : DEFAULT_MQTT_PASS;
        String tCmd = server.hasArg("mqtt_topic_cmd") ? server.arg("mqtt_topic_cmd") : DEFAULT_MQTT_TOPIC_CMD;
        String tState = server.hasArg("mqtt_topic_state") ? server.arg("mqtt_topic_state") : DEFAULT_MQTT_TOPIC_STATE;

        saveConfig(s_ssid, s_pass, srv, prt, usr, pwd, tCmd, tState);
        server.send(200, "text/html", "<h3>Settings Saved! Restarting ESP32...</h3><script>setTimeout(()=>{window.location='/'}, 4000);</script>");
        delay(1000);
        ESP.restart();
    });
    server.begin();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}

void loop() {
    checkButton();

    if (apModeActive) {
        server.handleClient();
        return;
    }

    checkWiFiConnection();

    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();

    webSocket.loop();
    server.handleClient();

    while (Serial.available()) {
        char c = Serial.read();
        lastCharTime = millis();
        if (serialBuffer.length() < 512) serialBuffer += c;
        else serialBuffer = "";
    }

    if (serialBuffer.length() > 0 && (millis() - lastCharTime >= SERIAL_TIMEOUT)) {
        processBufferedSerial(serialBuffer);
        serialBuffer = "";
    }
}
