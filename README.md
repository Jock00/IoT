# 🔔 Smart Video Doorbell - IoT End-to-End System (ESP32-CAM & MQTT)

![Status](https://img.shields.io/badge/Project_Status-Finished_MVP-success)
![Hardware](https://img.shields.io/badge/Hardware-ESP32--CAM-blue)
![Protocol](https://img.shields.io/badge/Protocol-MQTT_over_WiFi-orange)

## 📖 Overview

A complete access monitoring system built from scratch. This smart video doorbell captures visitor images and transmits them instantly to a custom dashboard without relying on paid cloud services or complex servers.

The main technical challenge was transmitting images (large payloads) through MQTT, a lightweight protocol optimized for short text messages. The solution uses a **Publisher/Subscriber** architecture with **Base64** encoding performed directly on the development board (**Edge Computing**).

---

## 📐 System Architecture

The system is **Event-Driven** (triggered only by button press to conserve energy) and follows this flow:

1. **Source (Publisher):** ESP32-CAM captures the image
2. **Transport (Broker):** MQTT server handles instant data transfer
3. **Destination (Subscriber):** Web/Mobile app displays the interface

### Data Flow##
[Button] → [ESP32: Capture & Base64 Encode] → [WiFi] → [MQTT Broker] → [Client: Decode & Notify]

---

## 🛠 Chapter 1: Hardware & Wiring

The physical device is a minimalist embedded system. GPIO pins 13 and 14 were chosen because most ESP32-CAM pins are internally reserved for the camera and SD card.

### Bill of Materials (BOM)
- **Processing Unit:** [ESP32-CAM AI-Thinker](https://docs.ai-thinker.com/en/esp32-cam)
- **Input:** Push Button (using internal **Input Pullup** resistor)
- **Output:** 5V Active Buzzer
- **Programming:** FTDI Adapter (USB-to-TTL)

### 🔌 Electrical Schematic

| Component | ESP32 Pin | Connection Type | Notes |
|:----------|:----------|:----------------|:------|
| **Button** | `GPIO 14` | Digital Input | `INPUT_PULLUP` mode (Active LOW) |
| **Buzzer** | `GPIO 13` | Digital Output | Active HIGH |
| **5V / GND** | 5V / GND | Power Supply | Stable source required to avoid **Brownout** |

---

## 💻 Chapter 2: Firmware & Optimizations

The code is written in C++ (PlatformIO). The biggest technical challenge was the MQTT packet size limitation.

### 1. MQTT Buffer Hack

The library's default buffer is 256 bytes. To send an image, we forced allocation of 60KB:
```cpp
void setup() {
  client.setServer(mqtt_server, mqtt_port);
  // ⚠️ Critical fix: Increase buffer size to handle Base64 images
  client.setBufferSize(60000); 
}
```

### 2. Image Processing (Edge Computing)

Convert binary image (.jpg) to text (Base64) directly on the processor:
```cpp
// Capture
camera_fb_t * fb = esp_camera_fb_get();

// Encode to Base64
String imageBase64 = base64::encode(fb->buf, fb->len);

// Publish to dedicated topic
client.publish("sonerie/poza", imageBase64.c_str());
```

### 3. Compression Configuration

Set `jpeg_quality = 20` (lower numbers mean better quality but larger files), finding the perfect balance between clarity and transmission speed.

---

## 📱 Chapter 3: Frontend & UI/UX

The user interface is a React Dashboard that handles display and notification challenges.

### 1. Serverless Rendering

Images aren't saved to disk. They're displayed directly from the received string using Data URI Scheme:
```javascript
// React / HTML example
<img 
  src={`data:image/jpeg;base64,${base64String}`} 
  alt="Live Capture" 
/>
```

### 2. Responsive Design (CSS Fix)

The "Default" card container had display issues on wide screens (too narrow). Fixed using Flexbox to force available space usage:
```css
/* Card container fix */
.card-container {
  display: flex;
  flex-direction: column;
  width: 100%;       /* Force available width usage */
  flex-grow: 1;      /* Allow expansion on main axis */
}
```

### 3. "Red Dot" Notification System 🔴

Visual State Management logic:

- Frontend listens to `sonerie/alerta` topic
- On message receipt, `hasUnread` state becomes `true`
- Interface renders red circle over menu
- On view, state resets

---

## ⚙️ Installation Guide

### Hardware Setup

1. Make connections according to the table above
2. For programming, connect GPIO 0 to GND
3. Upload code via FTDI adapter
4. **Important:** Disconnect GPIO 0 and press Reset

### Software Setup

1. Clone repository and open in VS Code / PlatformIO
2. Configure WiFi and MQTT Broker in `src/main.cpp`:
```cpp
const char *ssid = "Your_WiFi_Name";
const char *password = "Your_WiFi_Password";
const char* mqtt_server = "broker.emqx.io";
```

3. Upload code to ESP32-CAM
4. Start web application (`npm start`)

---

## 📝 Complete Firmware Code
```cpp
#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "board_config.h"
#include <PubSubClient.h>
#include <base64.h>

// ================= CONFIGURATION =================
const char *ssid = "Alex wifi";
const char *password = "alex1234";

// MQTT Broker
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

// Topics
const char* topic_poza = "sonerie/poza";
const char* topic_text = "sonerie/alerta";

#define BUTTON_PIN 14
#define BUZZER_PIN 13 
// ===============================================

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // --- Camera Configuration ---
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA; 
    config.jpeg_quality = 20; // Quality 20 (lower = better) to keep payload small
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_camera_init(&config);

  // --- WiFi ---
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\n✅ WiFi Connected!");

  // --- MQTT Setup ---
  client.setServer(mqtt_server, mqtt_port);
  // ⚠️ MAGIC TRICK: Huge buffer for Base64 (60KB)
  client.setBufferSize(60000); 
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("🔌 Connecting to MQTT...");
    String clientId = "ESP32Cam-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Success!");
    } else {
      delay(2000);
    }
  }
}

void sendPhotoMQTT() {
  Serial.println("📸 Capturing...");
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) { 
    Serial.println("❌ Camera Error"); 
    return; 
  }

  // Check if photo fits in buffer
  // (Base64 increases size by ~35%, so limit is lower)
  if (fb->len < 45000) {
    Serial.print("Converting to Base64... ");
    // Convert photo to text
    String imageBase64 = base64::encode(fb->buf, fb->len);
    Serial.print("Sending (Length: ");
    Serial.print(imageBase64.length());
    Serial.println(")...");

    if (client.publish(topic_poza, imageBase64.c_str())) {
      Serial.println("✅ Photo sent!");
      digitalWrite(BUZZER_PIN, HIGH); 
      delay(100); 
      digitalWrite(BUZZER_PIN, LOW);
    } else {
      Serial.println("❌ Too large for MQTT!");
    }
  } else {
    Serial.println("⚠️ Raw photo too large!");
  }

  esp_camera_fb_return(fb);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("🔔 Doorbell!");
      client.publish(topic_text, "Visitor at the door!");
      sendPhotoMQTT();
      while(digitalRead(BUTTON_PIN) == LOW) delay(10);
    }
  }
}
```

---

## 🐛 Troubleshooting

| Issue | Cause | Solution |
|:------|:------|:---------|
| **Brownout Error** | Voltage drops below 3.3V when WiFi starts | Use short, quality USB cable with stable power source |
| **Payload Too Large** | Image too detailed for buffer | Increase `jpeg_quality` to 25-30 |
| **Black Image** | Camera ribbon cable issue | Check FPC cable and black locking clip |
| **MQTT Connection Failed** | Broker unreachable or wrong credentials | Verify broker address and port 1883 |

---

## 📚 Resources & References

- [PubSubClient Library](https://github.com/knolleary/pubsubclient) - MQTT library used
- [MQTT Explorer](http://mqtt-explorer.com/) - Debugging tool for topic visualization
- [ESP32-CAM Documentation](https://docs.ai-thinker.com/en/esp32-cam) - Official hardware reference

---

## 🎯 Key Takeaways

- **Edge Computing:** Processing images locally reduces latency and cloud dependency
- **Protocol Optimization:** MQTT buffer manipulation enables large payload transmission
- **Event-Driven Design:** Power-efficient approach for IoT devices
- **Serverless Architecture:** Data URI scheme eliminates need for file storage

