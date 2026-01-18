/**
 * ============================================================================
 * ESP32-CAM Smart Doorbell System - University Project
 * ============================================================================
 * 
 * @file     main.cpp
 * @brief    Smart doorbell with real-time image capture and MQTT communication
 * @version  1.0.0
 * @date     2026-01-17
 * @author   Alexandru Bucurie
 * 
 * @description
 * This project implements an intelligent doorbell system using ESP32-CAM.
 * When the doorbell button is pressed, the system:
 * 1. Captures a photo of the visitor
 * 2. Sends it to a remote server via MQTT
 * 3. Waits for access decision (YES/NO)
 * 4. Controls door access accordingly
 * 
 * The system demonstrates integration of:
 * - ESP32-CAM hardware and camera control
 * - WiFi networking and MQTT protocol
 * - Real-time image processing and transmission
 * - IoT communication patterns (publish/subscribe)
 * - Hardware control (LEDs, buzzer, button input)
 * 
 * @hardware_components
 * - ESP32-CAM (AI Thinker module)
 * - Push button (doorbell trigger)
 * - Active buzzer (audio feedback)
 * - Red LED (locked/denied indicator)
 * - Green LED (unlocked/granted indicator)
 * 
 * @software_dependencies
 * - esp_camera.h (ESP32 camera driver)
 * - WiFi.h (ESP32 WiFi library)
 * - PubSubClient.h (MQTT client library)
 * - base64.h (Base64 encoding library)
 * 
 * @learning_objectives
 * - Understanding MQTT publish/subscribe architecture
 * - Image capture and transmission techniques
 * - Real-time embedded systems programming
 * - IoT communication protocols
 * - Hardware-software integration
 * 
 * ============================================================================
 */

#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "board_config.h"
#include <PubSubClient.h>
#include <base64.h>

// ============================================================================
// CONFIGURATION SECTION
// ============================================================================

// WiFi credentials
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

// MQTT Broker settings (using free public broker for testing)
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

// MQTT Topics - Communication channels between ESP32 and server
const char* topic_image    = "your_unique_id/doorbell/photo";      // ESP32 publishes photos here
const char* topic_alert    = "your_unique_id/doorbell/alert";      // ESP32 publishes alerts here
const char* topic_command  = "your_unique_id/doorbell/command"; 

// Hardware pin definitions
#define BUTTON_PIN 14  // Doorbell button (active LOW with pull-up)
#define BUZZER_PIN 13  // Buzzer for audio feedback
#define LED_RED    12  // Red LED (door locked state)
#define LED_GREEN  15  // Green LED (door unlocked state)

// Timing constants
#define BUTTON_DEBOUNCE_MS    50    // Button debounce delay
#define DOOR_UNLOCK_TIME_MS   4000  // How long door stays unlocked
#define BUZZER_CONFIRM_MS     50    // Photo capture confirmation beep
#define BUZZER_HAPPY_SHORT_MS 150   // Access granted - first beep
#define BUZZER_HAPPY_LONG_MS  400   // Access granted - second beep
#define ALARM_FLASH_COUNT     5     // Number of flashes for access denied
#define ALARM_FLASH_DELAY_MS  100   // Delay between alarm flashes

// Camera configuration
#define CAMERA_XCLK_FREQ_HZ   20000000  // Camera clock frequency
#define CAMERA_FB_COUNT       1         // Frame buffers (1 = always fresh capture)
#define CAMERA_QUALITY_PSRAM  25        // JPEG quality with PSRAM (lower = better)
#define CAMERA_QUALITY_NO_PSRAM 12      // JPEG quality without PSRAM
#define MQTT_BUFFER_SIZE      60000     // MQTT buffer size (60KB for images)
#define IMAGE_SIZE_LIMIT      60000     // Maximum image size

/**
 * Technical Notes:
 * 
 * 1. fb_count = 1 (Frame Buffer Count)
 *    - Single buffer mode ensures we always get a FRESH image
 *    - With fb_count=2, camera might return old/cached frame
 *    - For doorbell: fresh image is critical (visitor might have moved)
 * 
 * 2. JPEG Quality = 25
 *    - Scale: 0-63 (lower = better quality but larger file)
 *    - Quality 25 gives ~40-60KB images (clear faces, fits in buffer)
 *    - Balance between image clarity and transmission speed
 * 
 * 3. MQTT Buffer = 60KB
 *    - Default buffer is only 256 bytes (too small for images!)
 *    - 60KB handles VGA images with Base64 encoding (~33% overhead)
 *    - Must be set before connecting to MQTT broker
 * 
 * 4. Dummy Photo Technique (see sendPhotoMQTT function)
 *    - Take one throwaway photo to clear camera sensor
 *    - Then take real photo for transmission
 *    - Prevents "old image" problem from stale sensor data
 */

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

WiFiClient espClient;           // WiFi TCP/IP client
PubSubClient client(espClient); // MQTT client
bool isBusy = false;            // Prevents multiple simultaneous operations

// ============================================================================
// MQTT SUBSCRIBER - Receives Commands (YES/NO)
// ============================================================================

/**
 * callback() - MQTT Message Handler
 * 
 * This function is called automatically when a message arrives on a
 * subscribed topic (topic_command = "sonerie/comanda")
 * 
 * Flow:
 * 1. Server analyzes photo and decides: YES or NO
 * 2. Server PUBLISHES decision to "sonerie/comanda"
 * 3. MQTT broker forwards message to this ESP32
 * 4. This callback() function executes automatically
 * 5. Door opens (YES) or alarm sounds (NO)
 * 
 * Expected commands: "YES" or "NO"
 */
void callback(char* topic, byte* payload, unsigned int length) {
  // Convert byte array to string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("📩 Received command: ");
  Serial.println(message);

  // Process YES command - Grant access
  if (message == "YES") {
    Serial.println("✅ ACCESS GRANTED");
    
    // Visual: Red off, Green on
    digitalWrite(LED_RED, HIGH);    // OFF (active-low LED)
    digitalWrite(LED_GREEN, LOW);   // ON (active-low LED)
    
    // Audio: Happy two-tone sound
    digitalWrite(BUZZER_PIN, HIGH); delay(BUZZER_HAPPY_SHORT_MS); digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH); delay(BUZZER_HAPPY_LONG_MS); digitalWrite(BUZZER_PIN, LOW);
    
    // Keep door unlocked for 4 seconds
    delay(DOOR_UNLOCK_TIME_MS);
    
    // Return to locked state
    digitalWrite(LED_GREEN, HIGH);  // OFF
    digitalWrite(LED_RED, LOW);     // ON
  }
  // Process NO command - Deny access
  else if (message == "NO") {
    Serial.println("⛔ ACCESS DENIED");
    
    // Alarm: Flash red LED with buzzer 5 times
    for (int i = 0; i < ALARM_FLASH_COUNT; i++) {
      digitalWrite(LED_RED, HIGH); digitalWrite(BUZZER_PIN, LOW);  delay(ALARM_FLASH_DELAY_MS);
      digitalWrite(LED_RED, LOW);  digitalWrite(BUZZER_PIN, HIGH); delay(ALARM_FLASH_DELAY_MS);
    }
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_RED, LOW);  // Keep red LED on (locked state)
  }
  else {
    Serial.print("⚠️ Unknown command: ");
    Serial.println(message);
  }
}

// ============================================================================
// SETUP - Initialize System
// ============================================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  ESP32-CAM SMART DOORBELL - STARTING  ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Configure GPIO pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);  // Button with internal pull-up
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  // Initial state: locked (red on, green off)
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_GREEN, HIGH);  // OFF
  digitalWrite(LED_RED, LOW);     // ON
  
  Serial.println("⚙️  Hardware initialized");

  // Camera configuration
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
  config.xclk_freq_hz = CAMERA_XCLK_FREQ_HZ;
  config.pixel_format = PIXFORMAT_JPEG;
  config.fb_count = CAMERA_FB_COUNT;
  
  // Set resolution based on PSRAM availability
  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;      // 640x480 pixels
    config.jpeg_quality = CAMERA_QUALITY_PSRAM;
    Serial.println("📷 Camera: VGA (640x480) with PSRAM");
  } else {
    config.frame_size = FRAMESIZE_QVGA;     // 320x240 pixels
    config.jpeg_quality = CAMERA_QUALITY_NO_PSRAM;
    Serial.println("📷 Camera: QVGA (320x240) without PSRAM");
  }
  
  // Initialize camera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("❌ Camera initialization failed!");
    while(1) { delay(1000); }  // Halt system
  }
  Serial.println("✓ Camera ready");

  // Connect to WiFi
  Serial.print("📡 Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✓ WiFi connected");
  Serial.print("   IP: ");
  Serial.println(WiFi.localIP());

  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);  // Register callback for incoming messages
  
  if (!client.setBufferSize(MQTT_BUFFER_SIZE)) {
    Serial.println("❌ Failed to allocate MQTT buffer!");
  } else {
    Serial.printf("🔌 MQTT buffer: %d bytes\n", MQTT_BUFFER_SIZE);
  }
  
  Serial.println("\n✅ SYSTEM READY - Press doorbell to test\n");
  
  // Ready beep
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH); delay(80);
    digitalWrite(BUZZER_PIN, LOW); delay(80);
  }
}

// ============================================================================
// MQTT CONNECTION HANDLER
// ============================================================================

/**
 * reconnect() - Connect to MQTT broker and subscribe to command topic
 * 
 * SUBSCRIPTION happens here!
 * client.subscribe(topic_command) tells the broker:
 * "Send me all messages published to 'sonerie/comanda'"
 */
void reconnect() {
  while (!client.connected()) {
    Serial.print("🔌 Connecting to MQTT... ");
    
    String clientId = "ESP32Cam-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected!");
      
      // SUBSCRIBE to command topic - this is where we receive YES/NO
      if (client.subscribe(topic_command)) {
        Serial.printf("✓ Subscribed to: %s\n", topic_command);
        Serial.println("  → Listening for YES/NO commands\n");
      }
    } else {
      Serial.println("Failed, retrying...");
      delay(2000);
    }
  }
}

// ============================================================================
// MQTT PUBLISHER - Sends Photo
// ============================================================================

/**
 * sendPhotoMQTT() - Capture and send photo via MQTT
 * 
 * PUBLISHING happens here!
 * client.publish(topic_image, photo) sends the photo to the broker
 * 
 * Dummy Capture Technique:
 * 1. Take throwaway photo (clears camera sensor buffer)
 * 2. Take real photo (guaranteed fresh, current image)
 * 3. Encode to Base64 (MQTT needs text format)
 * 4. Publish to topic
 */
void sendPhotoMQTT() {
  camera_fb_t * fb = NULL;

  // Step 1: Dummy capture to clear buffer
  Serial.println("♻️  Clearing camera buffer...");
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb);  // Discard immediately

  // Step 2: Capture real photo
  Serial.println("📸 Capturing photo...");
  fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("❌ Camera capture failed!");
    isBusy = false;
    return;
  }
  
  Serial.printf("✓ Captured: %d bytes (%dx%d)\n", fb->len, fb->width, fb->height);

  // Step 3: Check size
  if (fb->len >= IMAGE_SIZE_LIMIT) {
    Serial.println("⚠️  Image too large!");
    esp_camera_fb_return(fb);
    isBusy = false;
    return;
  }

  // Step 4: Encode to Base64
  String imageBase64 = base64::encode(fb->buf, fb->len);
  Serial.printf("📦 Encoded: %d characters\n", imageBase64.length());

  // Step 5: PUBLISH to MQTT topic
  Serial.print("📤 Publishing to MQTT... ");
  if (client.publish(topic_image, imageBase64.c_str())) {
    Serial.println("✅ Sent!");
    client.loop();  // Process MQTT queue
    
    // Confirmation beep
    digitalWrite(BUZZER_PIN, HIGH);
    delay(BUZZER_CONFIRM_MS);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    Serial.println("❌ Failed to send!");
  }

  // Cleanup
  esp_camera_fb_return(fb);
}

// ============================================================================
// MAIN LOOP - Monitor Button and Handle MQTT
// ============================================================================

void loop() {
  // Maintain MQTT connection
  if (!client.connected()) {
    reconnect();
  }
  client.loop();  // Process incoming MQTT messages (triggers callback)

  // Check for button press
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(BUTTON_DEBOUNCE_MS);  // Debounce
    
    if (digitalRead(BUTTON_PIN) == LOW && !isBusy) {
      isBusy = true;
      
      Serial.println("\n🔔 DOORBELL PRESSED!");
      Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

      // PUBLISH alert
      if (client.publish(topic_alert, "Visitor!")) {
        Serial.println("✓ Alert sent");
      }
      
      // PUBLISH photo
      sendPhotoMQTT();
      
      isBusy = false;
      Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
      Serial.println("✅ Ready for next visitor\n");

      // Wait for button release
      while (digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
      }
      delay(50);
    }
  }
}
