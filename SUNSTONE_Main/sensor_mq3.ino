/*
 * SUNSTONE ESP32 - MQ-3 Ethanol Sensor
 * File: sensor_mq3.ino (Tab 8)
 * Pin: GPIO 35
 */

void setupMQ3() {
  pinMode(PIN_MQ3, INPUT);
  Serial.println("✓ MQ-3 initialized on GPIO 35");
}

int readMQ3() {
  return analogRead(PIN_MQ3);
}