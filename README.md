# ESP32-4ch_Relay_and_1_Fan

This ESP32 firmware project is a hybrid home automation controller designed to manage both digital relays (appliances/lights) and an AC phase-cut dimmer (such as a ceiling fan speed controller). It combines local hardware control interfaces with remote wireless management.

Here is a breakdown of its features:

1. Multi-Channel Relay & Fan Dimmer Control
4-Channel Appliance Switching: Controls 4 independent relay outputs (mapped to GPIOs 5, 18, 19, and 23) supporting active-LOW relay modules.

AC Phase-Cut Dimmer: Features a 10-stage dimmer (levels 0–9) using a Zero-Crossing Detection (ZCD) interrupt (ZeroCrossPin on GPIO 4) and a hardware timer to trigger a TRIAC gate driver (TriacPin on GPIO 25) at precise microsecond delays.

State Persistence: Automatically saves relay states, dimmer values, and configuration parameters to non-volatile flash memory using the ESP32 Preferences library so they survive reboots.

2. Multi-Modal Input & Control Interfaces
Built-in Web Dashboard & WebSockets: Hosts a responsive, modern web UI with real-time bidirectional communication via WebSockets (port 81). It gives live visual feedback on relay states and dimmer levels without requiring page refreshes.

Physical Wall Switches (6 Inputs):

4 momentary or toggle wall switches linked to toggle the individual relays.

2 dedicated pushbuttons for stepping the fan dimmer up or down (with long-press support to jump straight to MIN or MAX speed).

Infrared (IR) Remote Control: Decodes signals from an IR receiver (GPIO 16) to toggle individual relays, turn the dimmer on/off, adjust fan speed, or master-switch everything on/off.

Serial Command Console: Accepts command lines over the USB serial interface (115200 baud) with buffering to execute actions locally.

3. Cloud Integration & MQTT Telemetry
Secure MQTT Broker Client: Connects securely over TLS (WiFiClientSecure with insecure certificate validation) to an MQTT broker (pre-configured for an EMQX cloud broker instance).

Pub/Sub Automation: Subscribes to and publishes state updates and command messages, making it easy to integrate into larger home automation ecosystems like Home Assistant or Node-RED.

4. Robust Configuration & Fallback Mechanism
Soft-AP Configuration Portal: If Wi-Fi connection fails for longer than 10 minutes, or if the user long-presses the onboard Boot button (GPIO 0) for 3 seconds, the ESP32 drops into Access Point (AP) Mode (ESP32-Setup-AP).

Web-Based Config Panel: While in AP mode, users can connect directly via Wi-Fi to a dedicated configuration web page (/config) to update Wi-Fi credentials and MQTT broker details without needing to re-flash the code.
