/*
 * SUNSTONE ESP32 - MQ-7 CO Sensor
 * File: sensor_mq7.ino (Tab 7)
 * Pin: GPIO 33
 */

void setupMQ7() {
  pinMode(PIN_MQ7, INPUT);
  Serial.println("✓ MQ-7 initialized on GPIO 33");
}

int readMQ7() {
  return analogRead(PIN_MQ7);
}