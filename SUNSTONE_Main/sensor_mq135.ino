/*
 * SUNSTONE ESP32 - MQ-135 VOC Sensor
 * File: sensor_mq135.ino (Tab 9)
 * Pin: GPIO 32
 */

void setupMQ135() {
  pinMode(PIN_MQ135, INPUT);
  Serial.println("✓ MQ-135 initialized on GPIO 32");
}

int readMQ135() {
  return analogRead(PIN_MQ135);
}