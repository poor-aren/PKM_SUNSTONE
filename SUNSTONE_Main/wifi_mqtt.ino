/*
 * SUNSTONE ESP32 - WiFi & MQTT Functions
 * File: wifi_mqtt.ino (Tab 3)
 * 
 * FIXED: Reduced WiFi power to prevent brownout
 */

void setupWiFi() {
  Serial.println("\n=== WiFi Setup - Low Power Mode ===");
  
  // ✅ CRITICAL: Set WiFi mode and reduce power BEFORE begin
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_2dBm);  // ULTRA LOW POWER (was 11dBm)d:\KULIAH\PKM\SUNSTONE_Main\button_handler.ino
  Serial.println("✓ WiFi TX power: 2dBm (ultra low)");
  delay(300);
  
  // Start WiFi connection
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  delay(500);
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
    
    if (timeout % 5 == 0) {
      delay(300);
    }
  }
  
  Serial.println("");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi connected!");
    Serial.print("  SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("  IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("  Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("✗ WiFi connection failed!");
    Serial.println("  System will continue without WiFi");
  }
  
  Serial.println("=== WiFi Setup Complete ===\n");
}

void setupMQTT() {
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setKeepAlive(60);
  mqtt.setSocketTimeout(30);
  Serial.println("✓ MQTT configured");
}

void reconnectMQTT() {
  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt < 5000) return;
  
  lastAttempt = millis();
  Serial.print("Connecting MQTT...");
  
  String clientId = "SUNSTONE_" + String(DEVICE_ID) + "_" + String(random(0xffff), HEX);
  
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

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  String action = doc["action"] | "";
  Serial.print("MQTT Action: ");
  Serial.println(action);

  if (action == "session_ready") {
    hasActiveSession = true;
    currentSessionId = doc["session_id"] | "";
    currentPatientName = doc["patient_name"] | "";
    Serial.println("✓ Session ready: " + currentPatientName);
    
  } else if (action == "no_session") {
    hasActiveSession = false;
    currentSessionId = "";
    currentPatientName = "";
    if (currentState == STATE_SENDING) {
      currentState = STATE_READY;
    }
    Serial.println("⚠ No active session");
    
  } else if (action == "close_session") {
    hasActiveSession = false;
    currentState = STATE_SESSION_CLOSED;
    sendCounter = 0;
    buttonIndicator = false;
    Serial.println("✗ Session closed by web");
    
  } else if (action == "session_active") {
    hasActiveSession = true;
    currentSessionId = doc["session_id"] | "";
    Serial.println("✓ Session still active");
    
  } else if (action == "result") {
    resultCategory = doc["category"] | "";
    resultConfidence = doc["confidence"] | 0.0;
    currentState = STATE_SHOWING_RESULT;
    buttonIndicator = false;
    Serial.print("🔬 Result: ");
    Serial.print(resultCategory);
    Serial.print(" (");
    Serial.print(resultConfidence * 100);
    Serial.println("%)");
  }
}

void requestSessionStatus() {
  StaticJsonDocument<128> doc;
  doc["device_id"] = DEVICE_ID;
  doc["timestamp"] = millis();
  
  char buffer[128];
  serializeJson(doc, buffer);
  
  mqtt.publish(TOPIC_STATUS_REQ.c_str(), buffer);
  Serial.println("📡 Requested session status");
}

void sendSensorData() {
  if (!hasActiveSession) {
    currentState = STATE_READY;
    buttonIndicator = false;
    Serial.println("⚠ Cannot send: No active session");
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
    Serial.print("✓ Data sent #");
    Serial.print(sendCounter);
    Serial.print(" [");
    Serial.print(mg811Value);
    Serial.print(",");
    Serial.print(mq7Value);
    Serial.print(",");
    Serial.print(mq3Value);
    Serial.print(",");
    Serial.print(mq135Value);
    Serial.println("]");
  } else {
    Serial.println("✗ Failed to send data");
  }
}