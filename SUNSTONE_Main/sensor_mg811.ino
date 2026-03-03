/*
 * SUNSTONE ESP32 - MG-811 CO2 Sensor
 * File: sensor_mg811.ino (Tab 6)
 * Pin: GPIO 34
 * Nilai dibalik: 4095 - raw value
 */

void setupMG811() {
  pinMode(PIN_MG811, INPUT);
  Serial.println("✓ MG-811 initialized on GPIO 34");
}

int readMG811() {
  int rawValue = analogRead(PIN_MG811);
  int invertedValue = 4095 - rawValue;  // Nilai dibalik
  return invertedValue;
}