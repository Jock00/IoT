# 🔔 ESP32-CAM Smart Doorbell System

A smart doorbell project using ESP32-CAM and MQTT protocol for remote access control.

![ESP32-CAM](https://img.shields.io/badge/ESP32--CAM-AI%20Thinker-blue)
![MQTT](https://img.shields.io/badge/Protocol-MQTT-orange)
![PlatformIO](https://img.shields.io/badge/Platform-PlatformIO-orange)

---

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [System Architecture](#system-architecture)
- [Hardware Components](#hardware-components)
- [Circuit Diagram](#circuit-diagram)
- [Software Setup](#software-setup)
- [Configuration](#configuration)
- [MQTT Topics](#mqtt-topics)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)

---

## 🎯 Overview

Smart doorbell system that captures photos of visitors and sends them via MQTT protocol. Access control is managed remotely through a mobile application.

**Functionality:**
- Photo capture on button press
- Image transmission to MQTT broker
- Remote access control (YES/NO commands)
- Visual feedback (LED indicators)
- Audio feedback (buzzer)

---

## ✨ Features

- 📸 VGA resolution (640x480) image capture
- 🌐 WiFi connectivity
- 📱 Mobile app control via MQTT
- 💡 LED status indicators
- 🔊 Audio feedback system
- 🔘 Button debouncing

---

## 🏗️ System Architecture
```
     ┌───────────┐                            ┌────────────┐
     │  ESP32    │                            │   Mobile   │
     │   CAM     │                            │    App     │
     └─────┬─────┘                            └──────┬─────┘
           │                                         │
           │ PUBLISH                                 │ SUBSCRIBE
           │ (Photo)                                 │ (Photo)
           │                                         │
           ▼                                         ▼
┌─────────────────────────────────────────────────────────────────┐
│                    MQTT BROKER (Central Hub)                    │
│                     broker.emqx.io:1883                         │
└─────────────────────────────────────────────────────────────────┘
           ▲                                         │
           │                                         │
           │ SUBSCRIBE                               │ PUBLISH
           │ (YES/NO)                                │ (YES/NO)
           │                                         │
     ┌─────┴─────┐                            ┌──────┴─────┐
     │  ESP32    │                            │   Mobile   │
     │   CAM     │                            │    App     │
     └───────────┘                            └────────────┘
```

**Communication Flow:**
1. Button press triggers photo capture
2. ESP32-CAM publishes photo to MQTT broker
3. MQTT broker forwards photo to mobile app
4. User sends YES/NO command via mobile app
5. MQTT broker forwards command to ESP32-CAM
6. ESP32-CAM executes corresponding action

---

## 🛠️ Hardware Components

| Component | Specification | Quantity |
|-----------|--------------|----------|
| ESP32-CAM | AI Thinker with OV2640 | 1 |
| ESP32-CAM-MB | Programmer board | 1 |
| Push Button | Momentary switch | 1 |
| Active Buzzer | 5V | 1 |
| Red LED | 5mm | 1 |
| Green LED | 5mm | 1 |
| Resistors | 330Ω | 2 |
| Breadboard | Standard | 1 |
| Jumper Wires | Male-to-male | ~15 |
| Micro USB Cable | Data + Power | 1 |

---

## 🔌 Circuit Diagram

### Pin Connections
```
ESP32-CAM Pin → Component
─────────────────────────
GPIO 12       → 330Ω → Red LED (+) → GND
GPIO 13       → Buzzer (+) → GND
GPIO 14       → Push Button → GND
GPIO 15       → 330Ω → Green LED (+) → GND
GND           → Common Ground
```

### Wiring Diagram
```
                              USB Cable
                        (Power + Programming)
                                 ▼
                                 │ Micro USB
                      ┌──────────┴──────────┐
                      │                     │
                      │     ESP32-CAM       │
                      │       + MB          │
                      │                     │
                      │          GPIO 14 ───┼──── Push Button ──── GND 
                      │                     │
                      │          GPIO 15 ───┼──── 330Ω ──── Red GREEN (+) ──── GND
                      │                     │
                      │          GPIO 13 ───┼──── Buzzer (+) ──── GND
                      │                     │
                      │          GPIO 12 ───┼──── 330Ω ──── Green LED (+) ──── GND
                      │                     │
                      │          GND ───────┼──── GND (Common)
                      │                     │
                      └─────────────────────┘
```

---

## 📦 Software Setup

### PlatformIO Configuration

**platformio.ini:**
```ini
[env:esp32cam]
platform = espressif32
board = esp32cam
framework = arduino
monitor_speed = 115200
lib_deps =
    esp32-camera
    knolleary/PubSubClient
    densaugeo/base64
```

### Required Libraries
- `esp32-camera` - Camera driver
- `PubSubClient` - MQTT client
- `base64` - Image encoding

---

## ⚙️ Configuration

### WiFi Settings

Edit in `main.cpp`:
```cpp
const char *ssid = "WIFI_SSID";
const char *password = "WIFI_PASSWORD";
```

### MQTT Topics
```cpp
const char* topic_image    = "unique_id/doorbell/photo";
const char* topic_command  = "unique_id/doorbell/command";
```

**Note:** Change `unique_id` to avoid topic conflicts on public broker.

### Upload Process

1. Connect ESP32-CAM via USB
2. Click Upload in PlatformIO
3. Monitor serial output (115200 baud)

---

## 📡 MQTT Topics

| Topic | Publisher | Subscriber | Content |
|-------|-----------|------------|---------|
| `unique_id/doorbell/photo` | ESP32-CAM | Mobile App | Base64 encoded JPEG |
| `unique_id/doorbell/command` | Mobile App | ESP32-CAM | "YES" or "NO" |

**MQTT Broker:** `broker.emqx.io:1883` (public, no authentication)

---

## 📱 Testing

### Mobile App Setup

**Recommended Apps:**
- Android: "MQTT Dashboard", "IoT MQTT Panel"
- iOS: "MQTTool", "IoT MQTT Panel"

**Configuration:**
- Broker: `broker.emqx.io`
- Port: `1883`
- Subscribe: `unique_id/doorbell/photo`
- Publish: `unique_id/doorbell/command` with payload "YES" or "NO"

### Command Line Testing
```bash
# Subscribe to photos
mosquitto_sub -h broker.emqx.io -t "unique_id/doorbell/photo"

# Send commands
mosquitto_pub -h broker.emqx.io -t "unique_id/doorbell/command" -m "YES"
mosquitto_pub -h broker.emqx.io -t "unique_id/doorbell/command" -m "NO"
```

---

## 🐛 Troubleshooting

| Issue | Solution |
|-------|----------|
| Upload fails | Check COM port, try RESET button, verify USB cable |
| Camera error | Check ribbon cable connection |
| WiFi connection | Verify SSID/password, use 2.4GHz network |
| MQTT issues | Check internet, verify topic names (case-sensitive) |
| Image too large | Increase `CAMERA_QUALITY_PSRAM` value (25→30) |
| LEDs not working | Check 330Ω resistors, verify LED polarity |

---

## 📊 Technical Details

### Key Parameters

- **fb_count = 1**: Single buffer for fresh captures
- **JPEG Quality = 25**: Balance between size (~40-60KB) and clarity
- **MQTT Buffer = 60KB**: Accommodates Base64 encoded images
- **Dummy Capture**: Clears sensor before real capture

### LED Configuration
- Red LED: Locked/Denied state (active-low logic)
- Green LED: Unlocked/Granted state (active-low logic)
- 330Ω current-limiting resistors

---

## 📚 Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP32-CAM Pinout](https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/)
- [MQTT Protocol](https://mqtt.org/)
- [PubSubClient Library](https://github.com/knolleary/pubsubclient)

---

## 👨‍💻 Author

**Alexandru Bucurie**  
University Project  
IoT @UPB

---

## 📄 License

MIT License

---
