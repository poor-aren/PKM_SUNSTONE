/*
 * SUNSTONE ESP32 - Main Code
 * File: SUNSTONE_Main.ino (Tab 1)
 * 
 * FIXED: Sequential sensor activation + WiFi power reduction
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include "config.h"

// Global objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiClient espClient;
PubSubClient mqtt(espClient);

// System state variables
SystemState currentState = STATE_WARMUP;
unsigned long warmupStartTime = 0;
unsigned long lastSendTime = 0;
unsigned long lastSensorRead = 0;
unsigned long lastOLEDUpdate = 0;
const unsigned long OLED_UPDATE_INTERVAL = 1000;
const unsigned long SENSOR_READ_INTERVAL = 1000;

// ✅ SENSOR ACTIVATION STATE
enum SensorInitState {
  INIT_START,
  INIT_MG811,
  INIT_MQ7,
  INIT_MQ3,
  INIT_MQ135,
  INIT_COMPLETE
};

SensorInitState sensorInitState = INIT_START;
unsigned long sensorInitTimer = 0;
const unsigned long SENSOR_INIT_DELAY = 3000; // 3 detik per sensor

// Flags untuk sensor mana yang sudah aktif
bool mg811Active = false;
bool mq7Active = false;
bool mq3Active = false;
bool mq135Active = false;

// Session variables
bool hasActiveSession = false;
String currentSessionId = "";
String currentPatientName = "";

// Sensor values
int mg811Value = 0;
int mq7Value = 0;
int mq3Value = 0;
int mq135Value = 0;

// Result variables
String resultCategory = "";
float resultConfidence = 0.0;
int sendCounter = 0;

// Button variables
bool buttonPressed = false;
bool lastButtonReading = HIGH;
unsigned long lastDebounceTime = 0;
bool buttonIndicator = false;
unsigned long lastBlinkTime = 0;

// MQTT Topics
String TOPIC_SENSOR = "sunstone/esp32/" + String(DEVICE_ID) + "/sensor";
String TOPIC_CMD = "sunstone/esp32/" + String(DEVICE_ID) + "/cmd";
String TOPIC_STATUS_REQ = "sunstone/esp32/" + String(DEVICE_ID) + "/status_request";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("   SUNSTONE ESP32 - SAFE MODE");
  Serial.println("   Sequential Sensor Activation");
  Serial.println("=================================\n");

  // Step 1: OLED
  Serial.println("[1/5] OLED...");
  setupOLED();
  displayInitMessage("SUNSTONE", "Safe Mode", "OLED OK");
  delay(800);

  // Step 2: ADC
  Serial.println("[2/5] ADC...");
  analogSetAttenuation(ADC_11db);
  displayInitMessage("SUNSTONE", "Safe Mode", "ADC OK");
  delay(500);
  
  // Step 3: Button
  Serial.println("[3/5] Button...");
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  displayInitMessage("SUNSTONE", "Safe Mode", "Button OK");
  delay(500);

  // Step 4: WiFi (REDUCED POWER)
  Serial.println("[4/5] WiFi...");
  displayInitMessage("SUNSTONE", "Connecting...", "WiFi...");
  setupWiFi();
  delay(1000);

  // Step 5: MQTT
  Serial.println("[5/5] MQTT...");
  displayInitMessage("SUNSTONE", "Connecting...", "MQTT...");
  setupMQTT();
  delay(500);

  // Start sequential sensor init
  Serial.println("\n>>> Starting Sequential Sensor Init");
  Serial.println(">>> Each sensor: 3 second delay\n");
  sensorInitState = INIT_START;
  sensorInitTimer = millis();
  
  Serial.println("Core system ready\n");
}

void loop() {
  // Handle sequential sensor initialization
  handleSequentialSensorInit();
  
  // Maintain connections
  if (!mqtt.connected()) {
    reconnectMQTT();
  }
  mqtt.loop();

  // Handle inputs
  handleButton();
  
  // Read sensors (only active ones)
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL) {
    readAllSensors();
    lastSensorRead = millis();
  }

  // State machine
  switch (currentState) {
    case STATE_WARMUP:
      if (millis() - warmupStartTime >= WARMUP_DURATION) {
        currentState = STATE_READY;
        Serial.println("Warmup complete!");
        requestSessionStatus();
      }
      break;

    case STATE_READY:
      break;

    case STATE_SENDING:
      if (millis() - lastSendTime >= SEND_INTERVAL) {
        sendSensorData();
        lastSendTime = millis();
      }
      if (millis() - lastBlinkTime >= 500) {
        buttonIndicator = !buttonIndicator;
        lastBlinkTime = millis();
      }
      break;

    case STATE_SHOWING_RESULT:
      break;

    case STATE_SESSION_CLOSED:
      break;
  }

  // Update display
  if (millis() - lastOLEDUpdate >= OLED_UPDATE_INTERVAL) {
    if (sensorInitState == INIT_COMPLETE) {
      updateOLED();
    }
    lastOLEDUpdate = millis();
  }
  
  delay(50);
}

// ✅ SEQUENTIAL SENSOR INITIALIZATION
void handleSequentialSensorInit() {
  if (sensorInitState == INIT_COMPLETE) return;
  
  unsigned long elapsed = millis() - sensorInitTimer;
  
  switch (sensorInitState) {
    case INIT_START:
      displayInitMessage("SUNSTONE", "Sensor Init", "Starting...");
      sensorInitTimer = millis();
      sensorInitState = INIT_MG811;
      break;
      
    case INIT_MG811:
      if (elapsed >= SENSOR_INIT_DELAY) {
        Serial.println("[1/4] Activating MG-811...");
        setupMG811();
        mg811Active = true;
        displayInitMessage("SUNSTONE", "Sensor 1/4", "MG-811 OK");
        sensorInitTimer = millis();
        sensorInitState = INIT_MQ7;
      } else {
        displayInitMessage("SUNSTONE", "Sensor 1/4", "Wait: " + String(3 - elapsed/1000) + "s");
      }
      break;
      
    case INIT_MQ7:
      if (elapsed >= SENSOR_INIT_DELAY) {
        Serial.println("[2/4] Activating MQ-7...");
        setupMQ7();
        mq7Active = true;
        displayInitMessage("SUNSTONE", "Sensor 2/4", "MQ-7 OK");
        sensorInitTimer = millis();
        sensorInitState = INIT_MQ3;
      } else {
        displayInitMessage("SUNSTONE", "Sensor 2/4", "Wait: " + String(3 - elapsed/1000) + "s");
      }
      break;
      
    case INIT_MQ3:
      if (elapsed >= SENSOR_INIT_DELAY) {
        Serial.println("[3/4] Activating MQ-3...");
        setupMQ3();
        mq3Active = true;
        displayInitMessage("SUNSTONE", "Sensor 3/4", "MQ-3 OK");
        sensorInitTimer = millis();
        sensorInitState = INIT_MQ135;
      } else {
        displayInitMessage("SUNSTONE", "Sensor 3/4", "Wait: " + String(3 - elapsed/1000) + "s");
      }
      break;
      
    case INIT_MQ135:
      if (elapsed >= SENSOR_INIT_DELAY) {
        Serial.println("[4/4] Activating MQ-135...");
        setupMQ135();
        mq135Active = true;
        displayInitMessage("SUNSTONE", "Sensor 4/4", "MQ-135 OK");
        sensorInitTimer = millis();
        sensorInitState = INIT_COMPLETE;
        
        // Start warmup setelah semua sensor aktif
        Serial.println("\nAll sensors activated!");
        Serial.println("Starting 30s warmup...\n");
        warmupStartTime = millis();
        currentState = STATE_WARMUP;
      } else {
        displayInitMessage("SUNSTONE", "Sensor 4/4", "Wait: " + String(3 - elapsed/1000) + "s");
      }
      break;
  }
}

// Helper untuk display init message
void displayInitMessage(String line1, String line2, String line3) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println(line1);
  display.setCursor(0, 25);
  display.println(line2);
  display.setCursor(0, 40);
  display.println(line3);
  display.display();
}

// Read all sensors (only active ones)
void readAllSensors() {
  if (mg811Active) {
    mg811Value = readMG811();
  }
  
  if (mq7Active) {
    mq7Value = readMQ7();
  }
  
  if (mq3Active) {
    mq3Value = readMQ3();
  }
  
  if (mq135Active) {
    mq135Value = readMQ135();
  }
  
  // Print hanya jika semua sensor sudah aktif
  if (sensorInitState == INIT_COMPLETE) {
    Serial.print("MG811: ");
    Serial.print(mg811Value);
    Serial.print(" | MQ7: ");
    Serial.print(mq7Value);
    Serial.print(" | MQ3: ");
    Serial.print(mq3Value);
    Serial.print(" | MQ135: ");
    Serial.println(mq135Value);
  }
}