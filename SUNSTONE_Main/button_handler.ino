/*
 * SUNSTONE ESP32 - Button Handler
 * File: button_handler.ino (Tab 5)
 * 
 * Semua fungsi untuk handle button press
 */

void handleButton() {
  int reading = digitalRead(PIN_BUTTON);
  
  // Debug print every 2 seconds
  static unsigned long lastDebugPrint = 0;
  if (millis() - lastDebugPrint >= 2000) {
    Serial.print("Button: ");
    Serial.print(reading == LOW ? "PRESSED" : "RELEASED");
    Serial.print(" | State: ");
    Serial.print(currentState);
    Serial.print(" | Session: ");
    Serial.println(hasActiveSession ? "YES" : "NO");
    lastDebugPrint = millis();
  }
  
  // Debouncing
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
    lastButtonReading = reading;
  }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // Button pressed (LOW because INPUT_PULLUP)
    if (reading == LOW && !buttonPressed) {
      buttonPressed = true;
      onButtonPressed();
    }
    // Button released
    else if (reading == HIGH && buttonPressed) {
      buttonPressed = false;
      Serial.println("🔵 Button released");
    }
  }
}

void onButtonPressed() {
  Serial.println("\n🔴 ===== BUTTON PRESSED! =====");
  Serial.print("Current State: ");
  Serial.println(currentState);
  Serial.print("Has Active Session: ");
  Serial.println(hasActiveSession ? "YES" : "NO");
  
  if (currentState == STATE_READY && hasActiveSession) {
    // Start sending mode
    currentState = STATE_SENDING;
    sendCounter = 0;
    lastSendTime = 0;
    buttonIndicator = true;
    Serial.println("✅ Started sending mode");
    
  } else if (currentState == STATE_SENDING) {
    // Stop sending mode
    currentState = STATE_READY;
    buttonIndicator = false;
    Serial.println("⏹ Stopped sending mode");
    
  } else if (currentState == STATE_SHOWING_RESULT) {
    // Restart sending mode
    currentState = STATE_SENDING;
    sendCounter = 0;
    lastSendTime = 0;
    buttonIndicator = true;
    Serial.println("🔄 Restarted sending mode");
    
  } else if (currentState == STATE_SESSION_CLOSED) {
    // Request new session
    Serial.println("🔄 Requesting new session...");
    requestSessionStatus();
    currentState = STATE_READY;
    
  } else if (!hasActiveSession) {
    // No session available
    Serial.println("⚠ Cannot start: No active session");
    Serial.println("📡 Requesting session status...");
    requestSessionStatus();
    
  } else {
    Serial.println("⚠ Button pressed but no valid action");
  }
  
  Serial.println("===========================\n");
}