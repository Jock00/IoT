#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "board_config.h"
#include <PubSubClient.h>
#include <base64.h> // Libraria pentru traducerea pozei

// ================= CONFIGURARE =================
const char *ssid = "Alex wifi";
const char *password = "alex1234";

// Broker MQTT
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

// Topicuri
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

  // --- Configurare Cameră ---
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
    config.jpeg_quality = 20; // Calitate 20 (mai mica) pt a tine textul scurt
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  esp_camera_init(&config);

  // --- WiFi ---
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n✅ WiFi Conectat!");

  // --- MQTT Setup ---
  client.setServer(mqtt_server, mqtt_port);
  
  // ⚠️ TRUC MAGIC: Buffer urias pentru Base64 (60KB)
  client.setBufferSize(60000); 
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("🔌 Conectare MQTT...");
    String clientId = "ESP32Cam-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Reusit!");
    } else {
      delay(2000);
    }
  }
}

void sendPhotoMQTT() {
  Serial.println("📸 Captură...");
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb) { Serial.println("❌ Eroare Cameră"); return; }

  // Verificam daca poza incape in buffer
  // (Base64 mareste poza cu aprox 35%, deci limita e mai jos)
  if (fb->len < 45000) {
    Serial.print("Transform in Base64... ");
    
    // Traducem poza in text
    String imageBase64 = base64::encode(fb->buf, fb->len);
    
    Serial.print("Trimit (Length: ");
    Serial.print(imageBase64.length());
    Serial.println(")...");

    if (client.publish(topic_poza, imageBase64.c_str())) {
       Serial.println("✅ Poza trimisă!");
       digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
    } else {
       Serial.println("❌ Prea mare pentru MQTT!");
    }
  } else {
    Serial.println("⚠️ Poza bruta e prea mare!");
  }

  esp_camera_fb_return(fb);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("🔔 Sonerie!");
      client.publish(topic_text, "Vizitator la usa!");
      sendPhotoMQTT();
      while(digitalRead(BUTTON_PIN) == LOW) delay(10);
    }
  }
}