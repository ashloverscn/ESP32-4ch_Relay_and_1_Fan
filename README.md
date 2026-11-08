# ESP32 Hybrid Home Automation Controller

![ESP32](https://img.shields.io/badge/ESP32-WROOM-blue?logo=espressif)
![C++](https://img.shields.io/badge/C%2B%2B-Arduino-orange?logo=arduino)
![MQTT](https://img.shields.io/badge/MQTT-Enabled-green?logo=mqtt)
![License](https://img.shields.io/badge/License-MIT-yellow.svg)

A robust, feature-rich hybrid home automation firmware for the **ESP32-WROOM** designed to manage both digital relays (appliances and lights) and an AC phase-cut dimmer (such as a ceiling fan speed controller). It seamlessly bridges local hardware controls with powerful remote wireless management via WebSockets and secure MQTT.

---

## 🚀 Key Features

### 1. Multi-Channel Relay & Fan Dimmer Control
* **4-Channel Appliance Switching:** Controls 4 independent relay outputs supporting active-LOW relay modules.
* **AC Phase-Cut Dimmer:** Features a 10-stage dimmer (levels 0–9) utilizing a Zero-Crossing Detection (ZCD) interrupt and a hardware timer to trigger a TRIAC gate driver (e.g., MOC3021) at precise microsecond delays.
* **State Persistence:** Automatically saves relay states, dimmer values, and configuration parameters to non-volatile flash memory using the ESP32 `Preferences` library so they survive power cycles.

### 2. Multi-Modal Input & Control Interfaces
* **Built-in Web Dashboard & WebSockets:** Hosts a responsive, modern web dashboard with real-time bidirectional communication (`port 81`) providing live visual feedback without requiring page refreshes.
* **Physical Wall Switches (6 Inputs):** 
  * 4 momentary or toggle wall switches linked to individual relays.
  * 2 dedicated pushbuttons for stepping the fan dimmer up or down (featuring long-press support to instantly jump to MIN or MAX speed).
* **Infrared (IR) Remote Control:** Decodes signals from an IR receiver to toggle individual relays, turn the dimmer on/off, adjust fan speed, or master-switch everything on/off.
* **Serial Command Console:** Accepts command lines over the USB serial interface (`115200 baud`) with buffering for local debugging and automation scripts.

### 3. Cloud Integration & MQTT Telemetry
* **Secure MQTT Broker Client:** Connects securely over TLS (`WiFiClientSecure`) to cloud MQTT brokers (e.g., EMQX cloud).
* **Pub/Sub Automation:** Subscribes to and publishes state updates and command payloads, allowing seamless integration into smart home ecosystems like **Home Assistant** or **Node-RED**.

### 4. Robust Configuration & Fallback Mechanism
* **Soft-AP Configuration Portal:** If the Wi-Fi connection fails for longer than 10 minutes, or if you long-press the onboard Boot button (`GPIO 0`) for 3 seconds, the ESP32 automatically enters **Access Point (AP) Mode** (`ESP32-Setup-AP`).
* **Web-Based Config Panel:** Connect directly via Wi-Fi to the configuration web page (`/config`) to update Wi-Fi credentials and MQTT broker settings without needing to re-flash the code.

---

## 📌 Hardware Pinout Reference

| Component | Pin / GPIO | Description |
| :--- | :--- | :--- |
| **Relay 1** | GPIO 21 | Appliance / Light 1 |
| **Relay 2** | GPIO 18 | Appliance / Light 2 |
| **Relay 3** | GPIO 19 | Appliance / Light 3 |
| **Relay 4** | GPIO 23 | Appliance / Light 4 |
| **TRIAC Gate** | GPIO 25 | AC Dimmer Driver (MOC3021) |
| **Zero-Cross (ZCD)** | GPIO 4 | Zero-Crossing Detector (PC817) |
| **Wall Switch 1** | GPIO 34 | Input-only (Toggle Relay 1) |
| **Wall Switch 2** | GPIO 35 | Input-only (Toggle Relay 2) |
| **Wall Switch 3** | GPIO 36 (VP) | Input-only (Toggle Relay 3) |
| **Wall Switch 4** | GPIO 39 (VN) | Input-only (Toggle Relay 4) |
| **Dimmer Down** | GPIO 14 | Momentary pushbutton (Dimmer Down) |
| **Dimmer Up** | GPIO 27 | Momentary pushbutton (Dimmer Up) |
| **IR Receiver** | GPIO 16 | IR Demodulator Data Pin |
| **Config Button** | GPIO 0 | Onboard Boot button (AP Mode Trigger) |

---

## ⚙️ Getting Started & Configuration

1. **Clone the repository** and open it in **PlatformIO** or the **Arduino IDE**.
2. **Install Required Dependencies:**
   * `ArduinoJson`
   * `WebSockets` (by Markus Sattler)
   * `PubSubClient` (by Nick O'Leary)
   * `IRremote` (by Shirriff / Arduino-IRremote)
   * `AceButton`
3. **First Boot & Setup:**
   * On first boot (or if Wi-Fi credentials fail), the ESP32 enters AP mode.
   * Connect your phone or PC to the Wi-Fi network **`ESP32-Setup-AP`** (Password: `12345678`).
   * Open your browser and navigate to `http://192.168.4.1` or `http://192.168.4.1/config` to configure your local Wi-Fi SSID, Password, and MQTT broker details.
4. **Daily Operation:** Once connected to your local router, access the web control panel using the ESP32's assigned local IP address shown in your Serial Monitor.

---

## 🔌 Safety Warning

> [!WARNING]
> This project deals with **AC Mains Voltage (230V/120V)** and TRIAC phase-cut dimming. Always disconnect mains power before wiring relays, zero-crossing detectors, or dimmers. Exercise extreme caution, double-check all isolations, and use properly rated enclosures.

---

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.
