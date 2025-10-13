/*
 * SUNSTONE ESP32 - Lung Health Monitoring System
 * Hardware: ESP32 DevKit, OLED 2.42" I2C, 4x Gas Sensors, Rocker Switch
 * Integration: HiveMQ MQTT Broker + Web Dashboard
 * VERSION: 2.3 - UPDATED SENSOR CALIBRATION
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>

// ============================================
// OLED CONFIGURATION
// ============================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================
// PIN DEFINITIONS
// ============================================
#define PIN_MG811  34    // CO2 sensor
#define PIN_MQ7    33    // CO sensor
#define PIN_MQ3    35    // Ethanol sensor
#define PIN_MQ135  32    // VOC sensor
#define PIN_BUTTON 12    // Rocker switch (with internal pull-up)
#define PIN_SDA    21    // OLED SDA
#define PIN_SCL    22    // OLED SCL

// ============================================
// SENSOR CALIBRATION CONSTANTS
// ============================================
#define MG811_OFFSET  250    // Add 350
#define MQ7_OFFSET    -150   // Subtract 150
#define MQ3_OFFSET    -100   // Subtract 150
#define MQ135_OFFSET  -300   // Subtract 300

// ============================================
// WIFI MANAGER SETTINGS
// ============================================
const char* ap_ssid = "SUNSTONE WIFI MANAGER";
const char* ap_password = "sunstone";

WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

String stored_ssid = "";
String stored_password = "";

// ============================================
// MQTT CONFIGURATION (HIVEMQ PUBLIC BROKER)
// ============================================
const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char* DEVICE_ID = "ESP01";

// MQTT Topics
String TOPIC_SENSOR = "sunstone/esp32/" + String(DEVICE_ID) + "/sensor";
String TOPIC_CMD = "sunstone/esp32/" + String(DEVICE_ID) + "/cmd";
String TOPIC_STATUS_REQ = "sunstone/esp32/" + String(DEVICE_ID) + "/status_request";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// ============================================
// SYSTEM STATE
// ============================================
enum SystemState {
  STATE_WARMUP,
  STATE_READY,
  STATE_SENDING,
  STATE_SHOWING_RESULT,
  STATE_SESSION_CLOSED
};

SystemState currentState = STATE_WARMUP;
unsigned long warmupStartTime = 0;
const unsigned long WARMUP_DURATION = 30000; // 30 seconds
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 5000; // 5 seconds

// Session data
bool hasActiveSession = false;
String currentSessionId = "";
String currentPatientName = "";

// Sensor data
int mg811Value = 0;
int mq7Value = 0;
int mq3Value = 0;
int mq135Value = 0;

// Result data
String resultCategory = "";
float resultConfidence = 0.0;
int sendCounter = 0;

// Button state - IMPROVED DEBOUNCING
bool buttonPressed = false;
bool lastButtonReading = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 100;

// Button animation
bool buttonIndicator = false;
unsigned long lastBlinkTime = 0;

// ============================================
// HTML PAGES FOR WIFI MANAGER
// ============================================
const String HTML_HEAD = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset='utf-8'>
    <meta name='viewport' content='width=device-width,initial-scale=1'>
    <title>SUNSTONE WIFI MANAGER</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Arial, sans-serif;
            background: white;
            color: #333;
            line-height: 1.6;
        }
        .container {
            max-width: 500px;
            margin: 0 auto;
            padding: 20px;
        }
        .header {
            background: #1e3a8a;
            color: white;
            text-align: center;
            padding: 25px 20px;
            margin-bottom: 30px;
            border-radius: 8px;
            box-shadow: 0 4px 8px rgba(30, 58, 138, 0.2);
        }
        .header h1 {
            font-size: 24px;
            font-weight: 600;
            letter-spacing: 1px;
        }
        .card {
            background: white;
            border: 2px solid #1e3a8a;
            border-radius: 8px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        .card h2 {
            color: #1e3a8a;
            margin-bottom: 20px;
            font-size: 18px;
            border-bottom: 2px solid #1e3a8a;
            padding-bottom: 10px;
        }
        .wifi-item {
            padding: 15px;
            border: 1px solid #e5e7eb;
            margin-bottom: 10px;
            border-radius: 6px;
            cursor: pointer;
            transition: all 0.3s;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .wifi-item:hover {
            border-color: #1e3a8a;
            background: #f8faff;
        }
        .wifi-name {
            font-weight: 500;
            color: #1e3a8a;
        }
        .wifi-strength {
            font-size: 12px;
            color: #6b7280;
        }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 12px;
            border: 2px solid #e5e7eb;
            border-radius: 6px;
            font-size: 16px;
            transition: border-color 0.3s;
            margin-bottom: 15px;
        }
        input[type="text"]:focus, input[type="password"]:focus {
            outline: none;
            border-color: #1e3a8a;
        }
        .btn {
            background: #1e3a8a;
            color: white;
            border: none;
            padding: 12px 25px;
            border-radius: 6px;
            cursor: pointer;
            font-size: 16px;
            font-weight: 500;
            transition: background 0.3s;
            width: 100%;
            margin-bottom: 10px;
        }
        .btn:hover {
            background: #1e40af;
        }
        .btn-secondary {
            background: white;
            color: #1e3a8a;
            border: 2px solid #1e3a8a;
        }
        .btn-secondary:hover {
            background: #1e3a8a;
            color: white;
        }
        .status {
            padding: 15px;
            border-radius: 6px;
            margin-bottom: 20px;
            text-align: center;
        }
        .status.success {
            background: #f0fdf4;
            color: #166534;
            border: 1px solid #bbf7d0;
        }
        .status.error {
            background: #fef2f2;
            color: #dc2626;
            border: 1px solid #fecaca;
        }
        .info {
            background: #f8faff;
            padding: 15px;
            border-radius: 6px;
            border: 1px solid #1e3a8a;
            margin-bottom: 20px;
        }
        .loading {
            text-align: center;
            color: #6b7280;
            padding: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>SUNSTONE WIFI MANAGER</h1>
        </div>
)";

const String HTML_FOOT = R"(
    </div>
</body>
</html>
)";

// ============================================
// FUNCTION DECLARATIONS
// ============================================
void setupOLED();
void setupWiFi();
void setupWebServer();
void handleRoot();
void handleScan();
void handleConnect();
void handleSettings();
void handleReset();
void startAPMode();
void connectToWiFi();
void checkWiFiConnection();
void setupMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
void readSensors();
void sendSensorData();
void updateOLED();
void handleButton();
void requestSessionStatus();
void displayWarmup();
void displayReady();
void displaySending();
void displayResult();
void displaySessionClosed();
void drawButtonIndicator();
int calibrateSensor(int rawValue, int offset, int minValue, int maxValue);

// ============================================
// SENSOR CALIBRATION FUNCTION
// ============================================
int calibrateSensor(int rawValue, int offset, int minValue, int maxValue) {
  int calibratedValue = rawValue + offset;
  
  // Constrain to valid range
  if (calibratedValue < minValue) {
    return minValue;
  }
  if (calibratedValue > maxValue) {
    return maxValue;
  }
  
  return calibratedValue;
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n=== SUNSTONE ESP32 Initializing ===");

  // Initialize OLED
  setupOLED();
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SUNSTONE");
  display.println("Initializing...");
  display.display();

  // Initialize pins
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_MG811, INPUT);
  pinMode(PIN_MQ7, INPUT);
  pinMode(PIN_MQ3, INPUT);
  pinMode(PIN_MQ135, INPUT);

  Serial.print("Button test at startup: ");
  Serial.println(digitalRead(PIN_BUTTON) == LOW ? "PRESSED" : "RELEASED");

  // Initialize preferences
  preferences.begin("wifi", false);
  stored_ssid = preferences.getString("ssid", "");
  stored_password = preferences.getString("password", "");

  // Setup WiFi with Advanced Manager
  setupWiFi();

  // Setup MQTT
  setupMQTT();

  // Start warmup
  warmupStartTime = millis();
  currentState = STATE_WARMUP;
  
  Serial.println("=== System Ready - Starting Warmup ===");
  Serial.println("Button Pin: GPIO12");
  Serial.println("Button Mode: INPUT_PULLUP (Active LOW)");
  Serial.println("=== SENSOR CALIBRATION ACTIVE ===");
  Serial.println("MG-811: +350 | MQ-7: -150 | MQ-3: -150 | MQ-135: -300");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  // Handle WiFi Manager
  dnsServer.processNextRequest();
  server.handleClient();

  // Maintain MQTT connection
  if (!mqtt.connected()) {
    reconnectMQTT();
  }
  mqtt.loop();

  // Handle button press
  handleButton();

  // Read sensors continuously
  readSensors();

  // State machine
  switch (currentState) {
    case STATE_WARMUP:
      if (millis() - warmupStartTime >= WARMUP_DURATION) {
        currentState = STATE_READY;
        Serial.println("Warmup complete! System ready.");
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

  // Update OLED display
  updateOLED();

  delay(50);
}

// ============================================
// OLED SETUP
// ============================================
void setupOLED() {
  Wire.begin(PIN_SDA, PIN_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED initialization failed!");
    while (1);
  }
  display.clearDisplay();
  display.display();
  Serial.println("OLED initialized");
}

// ============================================
// WIFI SETUP WITH ADVANCED WIFI MANAGER
// ============================================
void setupWiFi() {
  // Start AP mode first
  startAPMode();
  
  // Try to connect to stored WiFi if available
  if (stored_ssid.length() > 0) {
    connectToWiFi();
  }
  
  // Setup web server for WiFi management
  setupWebServer();
}

void startAPMode() {
  Serial.println("Starting AP Mode...");
  
  // FIXED: Set mode to AP_STA explicitly before creating AP
  WiFi.mode(WIFI_AP_STA);
  delay(100); // Give time for mode change
  
  // Configure AP settings
  WiFi.softAPConfig(
    IPAddress(192, 168, 4, 1),  // AP IP
    IPAddress(192, 168, 4, 1),  // Gateway
    IPAddress(255, 255, 255, 0) // Subnet
  );
  
  // Start Access Point
  bool apStarted = WiFi.softAP(ap_ssid, ap_password);
  
  if (apStarted) {
    Serial.println("✅ AP Mode started successfully");
  } else {
    Serial.println("❌ AP Mode failed to start");
  }
  
  // Start DNS server for captive portal
  dnsServer.start(53, "*", WiFi.softAPIP());
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("AP SSID: " + String(ap_ssid));
  Serial.println("AP Password: " + String(ap_password));
  Serial.println("Connect and go to: http://192.168.4.1");
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("WiFi Manager");
  display.println("Mode Aktif!");
  display.println("");
  display.println("Connect to:");
  display.println(ap_ssid);
  display.println("");
  display.println("Pass: sunstone");
  display.println("IP: 192.168.4.1");
  display.display();
  delay(3000);
}

void connectToWiFi() {
  Serial.println("Attempting to connect to WiFi...");
  WiFi.begin(stored_ssid.c_str(), stored_password.c_str());
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting WiFi");
  display.print("SSID: ");
  display.println(stored_ssid);
  display.display();
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
    
    dnsServer.processNextRequest();
    server.handleClient();
    
    display.print(".");
    display.display();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected to: ");
    Serial.println(stored_ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("AP still active at: ");
    Serial.println(WiFi.softAPIP());
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.print("SSID: ");
    display.println(stored_ssid);
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.display();
    delay(2000);
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi");
    Serial.print("AP still active at: ");
    Serial.println(WiFi.softAPIP());
  }
}

// ============================================
// WEB SERVER SETUP
// ============================================
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/connect", handleConnect);
  server.on("/settings", handleSettings);
  server.on("/reset", handleReset);
  server.onNotFound(handleRoot);
  
  server.begin();
  Serial.println("Web server started");
}

void handleRoot() {
  String html = HTML_HEAD;
  
  if (WiFi.status() == WL_CONNECTED) {
    html += "<div class='status success'>";
    html += "✓ Terhubung ke: <strong>" + WiFi.SSID() + "</strong><br>";
    html += "IP Address: <strong>" + WiFi.localIP().toString() + "</strong>";
    html += "</div>";
  } else {
    html += "<div class='status error'>";
    html += "⚠ Tidak terhubung ke WiFi";
    html += "</div>";
  }
  
  html += "<div class='card'>";
  html += "<h2>🔗 Koneksi WiFi</h2>";
  html += "<form action='/connect' method='POST'>";
  html += "<input type='text' name='ssid' placeholder='Nama WiFi (SSID)' value='" + stored_ssid + "' required>";
  html += "<input type='password' name='password' placeholder='Password WiFi'>";
  html += "<button type='submit' class='btn'>Connect to WiFi</button>";
  html += "</form>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>⚙️ Pengaturan</h2>";
  html += "<button class='btn btn-secondary' onclick='location.href=\"/settings\"'>WiFi Settings</button>";
  html += "<button class='btn btn-secondary' onclick='resetWifi()' style='background:#dc2626; color:white; border-color:#dc2626;'>Reset WiFi</button>";
  html += "</div>";
  
  html += R"(
  <script>
    function resetWifi() {
      if(confirm('Yakin ingin reset pengaturan WiFi?')) {
        fetch('/reset').then(() => location.reload());
      }
    }
  </script>
  )";
  
  html += HTML_FOOT;
  server.send(200, "text/html", html);
}

void handleScan() {
  // REMOVED - Scan WiFi feature disabled
  server.send(200, "text/html", "Feature disabled");
}

void handleConnect() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  
  if (ssid.length() == 0) {
    server.send(400, "text/html", "SSID tidak boleh kosong");
    return;
  }
  
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  stored_ssid = ssid;
  stored_password = password;
  
  String html = HTML_HEAD;
  html += "<div class='status success'>";
  html += "⏳ Menghubungkan ke: <strong>" + ssid + "</strong><br>";
  html += "Tunggu beberapa detik...";
  html += "</div>";
  html += "<script>setTimeout(function(){ location.href='/'; }, 5000);</script>";
  html += HTML_FOOT;
  
  server.send(200, "text/html", html);
  
  connectToWiFi();
}

void handleSettings() {
  String html = HTML_HEAD;
  
  html += "<div class='info'>";
  html += "<strong>Informasi Device:</strong><br>";
  html += "Access Point: " + String(ap_ssid) + "<br>";
  html += "IP AP: " + WiFi.softAPIP().toString() + "<br>";
  if (WiFi.status() == WL_CONNECTED) {
    html += "WiFi Terhubung: " + WiFi.SSID() + "<br>";
    html += "IP WiFi: " + WiFi.localIP().toString() + "<br>";
  }
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h2>⚙️ Pengaturan WiFi</h2>";
  html += "<p>SSID Tersimpan: <strong>" + stored_ssid + "</strong></p>";
  html += "<button class='btn' onclick='location.href=\"/\"'>Kembali ke Home</button>";
  html += "</div>";
  
  html += HTML_FOOT;
  server.send(200, "text/html", html);
}

void handleReset() {
  preferences.clear();
  stored_ssid = "";
  stored_password = "";
  
  server.send(200, "text/html", "OK");
  
  delay(1000);
  ESP.restart();
}

void checkWiFiConnection() {
  // Keep checking WiFi status
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    if (WiFi.status() != WL_CONNECTED && stored_ssid.length() > 0) {
      connectToWiFi();
    }
    lastCheck = millis();
  }
}

// ============================================
// MQTT SETUP
// ============================================
void setupMQTT() {
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setKeepAlive(60);
  mqtt.setSocketTimeout(30);
}

void reconnectMQTT() {
  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt < 5000) return;
  
  lastAttempt = millis();
  
  Serial.print("Connecting to MQTT...");
  
  String clientId = "SUNSTONE_ESP32_" + String(DEVICE_ID) + "_" + String(random(0xffff), HEX);
  
  if (mqtt.connect(clientId.c_str())) {
    Serial.println("Connected!");
    
    mqtt.subscribe(TOPIC_CMD.c_str());
    Serial.println("Subscribed to: " + TOPIC_CMD);
    
    requestSessionStatus();
    
  } else {
    Serial.print("Failed, rc=");
    Serial.println(mqtt.state());
  }
}

// ============================================
// MQTT CALLBACK
// ============================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message from: ");
  Serial.println(topic);
  
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  String action = doc["action"] | "";
  Serial.print("Action: ");
  Serial.println(action);

  if (action == "session_ready") {
    hasActiveSession = true;
    currentSessionId = doc["session_id"] | "";
    currentPatientName = doc["patient_name"] | "";
    Serial.println("✅ Session ready: " + currentPatientName);
    
  } else if (action == "no_session") {
    hasActiveSession = false;
    currentSessionId = "";
    currentPatientName = "";
    if (currentState == STATE_SENDING) {
      currentState = STATE_READY;
    }
    Serial.println("⚠️ No active session");
    
  } else if (action == "close_session") {
    hasActiveSession = false;
    currentState = STATE_SESSION_CLOSED;
    sendCounter = 0;
    buttonIndicator = false;
    Serial.println("🔴 Session closed by web");
    
  } else if (action == "session_active") {
    hasActiveSession = true;
    currentSessionId = doc["session_id"] | "";
    Serial.println("✅ Session still active");
    
  } else if (action == "result") {
    resultCategory = doc["category"] | "";
    resultConfidence = doc["confidence"] | 0.0;
    currentState = STATE_SHOWING_RESULT;
    buttonIndicator = false;
    Serial.print("🔬 Result received: ");
    Serial.print(resultCategory);
    Serial.print(" (");
    Serial.print(resultConfidence * 100);
    Serial.println("%)");
  }
}

// ============================================
// REQUEST SESSION STATUS
// ============================================
void requestSessionStatus() {
  StaticJsonDocument<128> doc;
  doc["device_id"] = DEVICE_ID;
  doc["timestamp"] = millis();
  
  char buffer[128];
  serializeJson(doc, buffer);
  
  mqtt.publish(TOPIC_STATUS_REQ.c_str(), buffer);
  Serial.println("📡 Requested session status");
}

// ============================================
// READ SENSORS WITH CALIBRATION
// ============================================
void readSensors() {
  // Read raw sensor values
  int rawMG811 = analogRead(PIN_MG811);
  int rawMQ7 = analogRead(PIN_MQ7);
  int rawMQ3 = analogRead(PIN_MQ3);
  int rawMQ135 = analogRead(PIN_MQ135);
  
  // Apply calibration with range constraints
  mg811Value = calibrateSensor(rawMG811, MG811_OFFSET, 0, 4095);
  mq7Value = calibrateSensor(rawMQ7, MQ7_OFFSET, 0, 4095);
  mq3Value = calibrateSensor(rawMQ3, MQ3_OFFSET, 0, 4095);
  mq135Value = calibrateSensor(rawMQ135, MQ135_OFFSET, 0, 4095);
}

// ============================================
// SEND SENSOR DATA
// ============================================
void sendSensorData() {
  if (!hasActiveSession) {
    Serial.println("⚠️ Cannot send: No active session");
    currentState = STATE_READY;
    buttonIndicator = false;
    return;
  }

  StaticJsonDocument<256> doc;
  doc["device_id"] = DEVICE_ID;
  doc["session_id"] = currentSessionId;
  doc["mg811"] = mg811Value;
  doc["mq7"] = mq7Value;
  doc["mq3"] = mq3Value;
  doc["mq135"] = mq135Value;
  doc["ts"] = millis();
  
  char buffer[256];
  serializeJson(doc, buffer);
  
  bool success = mqtt.publish(TOPIC_SENSOR.c_str(), buffer, false);
  
  if (success) {
    sendCounter++;
    Serial.print("✅ Data sent #");
    Serial.print(sendCounter);
    Serial.print(": MG811=");
    Serial.print(mg811Value);
    Serial.print(", MQ7=");
    Serial.print(mq7Value);
    Serial.print(", MQ3=");
    Serial.print(mq3Value);
    Serial.print(", MQ135=");
    Serial.println(mq135Value);
  } else {
    Serial.println("❌ Failed to send data");
  }
}

// ============================================
// HANDLE BUTTON
// ============================================
void handleButton() {
  int reading = digitalRead(PIN_BUTTON);
  
  static unsigned long lastDebugPrint = 0;
  if (millis() - lastDebugPrint >= 2000) {
    Serial.print("Button state: ");
    Serial.print(reading == LOW ? "PRESSED" : "RELEASED");
    Serial.print(" | Current State: ");
    Serial.print(currentState);
    Serial.print(" | Has Session: ");
    Serial.println(hasActiveSession ? "YES" : "NO");
    lastDebugPrint = millis();
  }
  
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
    lastButtonReading = reading;
  }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading == LOW && !buttonPressed) {
      buttonPressed = true;
      
      Serial.println("\n🔴 ===== BUTTON PRESSED! =====");
      Serial.print("Current State: ");
      Serial.println(currentState);
      Serial.print("Has Active Session: ");
      Serial.println(hasActiveSession ? "YES" : "NO");
      
      if (currentState == STATE_READY && hasActiveSession) {
        currentState = STATE_SENDING;
        sendCounter = 0;
        lastSendTime = 0;
        buttonIndicator = true;
        Serial.println("✅ Started sending mode");
        
      } else if (currentState == STATE_SENDING) {
        currentState = STATE_READY;
        buttonIndicator = false;
        Serial.println("⏹️ Stopped sending mode");
        
      } else if (currentState == STATE_SHOWING_RESULT) {
        currentState = STATE_SENDING;
        sendCounter = 0;
        lastSendTime = 0;
        buttonIndicator = true;
        Serial.println("🔄 Restarted sending mode");
        
      } else if (currentState == STATE_SESSION_CLOSED) {
        Serial.println("🔄 Requesting new session...");
        requestSessionStatus();
        currentState = STATE_READY;
        
      } else if (!hasActiveSession) {
        Serial.println("⚠️ Cannot start: No active session");
        Serial.println("📡 Requesting session status...");
        requestSessionStatus();
        
      } else {
        Serial.println("⚠️ Button pressed but no valid action");
      }
      
      Serial.println("===========================\n");
    }
    else if (reading == HIGH && buttonPressed) {
      buttonPressed = false;
      Serial.println("🔵 Button released");
    }
  }
}

// ============================================
// DRAW BUTTON INDICATOR (TOP RIGHT CORNER)
// ============================================
void drawButtonIndicator() {
  if (currentState == STATE_SENDING && buttonIndicator) {
    display.fillCircle(120, 4, 3, SSD1306_WHITE);
  } else if (currentState == STATE_SENDING) {
    display.drawCircle(120, 4, 3, SSD1306_WHITE);
  }
}

// ============================================
// UPDATE OLED DISPLAY - FIXED LAYOUT
// ============================================
void updateOLED() {
  display.clearDisplay();
  
  // Top: WiFi status
  display.setTextSize(1);
  display.setCursor(0, 0);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("WiFi:");
    String ssid = WiFi.SSID();
    if (ssid.length() > 8) {
      display.print(ssid.substring(0, 8));
    } else {
      display.print(ssid);
    }
  } else {
    display.print("WiFi: ---");
  }
  
  // Draw button indicator in top right
  drawButtonIndicator();
  
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  
  // Middle: State display (area y=14 to y=47)
  display.setCursor(0, 14);
  
  switch (currentState) {
    case STATE_WARMUP:
      displayWarmup();
      break;
    case STATE_READY:
      displayReady();
      break;
    case STATE_SENDING:
      displaySending();
      break;
    case STATE_SHOWING_RESULT:
      displayResult();
      break;
    case STATE_SESSION_CLOSED:
      displaySessionClosed();
      break;
  }
  
  // Bottom: Sensor values (y=52 onwards)
  display.drawLine(0, 48, 127, 48, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print("[");
  display.print(mg811Value);
  display.print(",");
  display.print(mq7Value);
  display.print(",");
  display.print(mq3Value);
  display.print(",");
  display.print(mq135Value);
  display.print("]");
  
  display.display();
}

// ============================================
// DISPLAY FUNCTIONS - FIXED SIZES & POSITIONS
// ============================================

void displayWarmup() {
  unsigned long remaining = WARMUP_DURATION - (millis() - warmupStartTime);
  int seconds = remaining / 1000;
  
  display.setTextSize(1);
  display.println("Burning sensor:");
  display.println("");
  display.setTextSize(1);  // FIXED: Size 1 instead of 2
  display.print("Time: ");
  display.print(seconds);
  display.println("s");
}

void displayReady() {
  display.setTextSize(1);
  if (hasActiveSession) {
    display.println("Sistem siap!");
    display.println("Pasien:");
    String displayName = currentPatientName;
    if (displayName.length() > 16) {
      displayName = displayName.substring(0, 16);
    }
    display.println(displayName);
    display.setCursor(0, 38);
    display.println("Tekan tombol");
  } else {
    display.println("Siap");
    display.println("");
    display.println("Tidak ada sesi");
    display.println("");
    display.setCursor(0, 38);
    display.println("Cek sesi: tekan");
  }
}

void displaySending() {
  display.setTextSize(1);
  display.println("Mengirim data...");
  display.println("");
  
  // FIXED: Panah kecil ke samping + angka counter
  display.print("-> ");
  display.println(sendCounter);
}

void displayResult() {
  display.setTextSize(1);
  display.setCursor(0, 14);
  display.println("Hasil Analisis:");
  
  display.setCursor(0, 24);
  display.setTextSize(1);
  if (resultCategory.length() > 16) {
    display.println(resultCategory.substring(0, 16));
  } else {
    display.println(resultCategory);
  }
  
  display.setCursor(0, 34);
  display.setTextSize(1);
  display.print("Conf: ");
  display.print((int)(resultConfidence * 100));
  display.print("%");
}

void displaySessionClosed() {
  display.setTextSize(1);
  display.println("Sesi Selesai");
  display.println("");
  display.println("Tekan tombol utk");
  display.println("cek sesi baru");
}