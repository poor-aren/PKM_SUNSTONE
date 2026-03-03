/*
 * SUNSTONE ESP32 - Configuration File
 * File: config.h (Tab 2)
 * 
 * Semua konstanta, pin definitions, dan enums
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// OLED CONFIGURATION
// ============================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

// ============================================
// PIN DEFINITIONS
// ============================================
#define PIN_MG811  34    // CO2 sensor
#define PIN_MQ7    33    // CO sensord:\KULIAH\PKM\SUNSTONE_Main\wifi_mqtt.ino
#define PIN_MQ3    35    // Ethanol sensor
#define PIN_MQ135  32    // VOC sensor
#define PIN_BUTTON 12    // Rocker switch
#define PIN_SDA    21    // OLED SDA
#define PIN_SCL    22    // OLED SCL

// ============================================
// WIFI CONFIGURATION
// ============================================
const char* ssid = "sunstone";
const char* password = "sunstone";

// ============================================
// MQTT CONFIGURATION
// ============================================
const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char* DEVICE_ID = "ESP01";

// ============================================
// TIMING CONSTANTS
// ============================================
const unsigned long WARMUP_DURATION = 30000;  // 30 seconds
const unsigned long SEND_INTERVAL = 1000;     // 1 detik (kirim data per detik saat tombol ditekan)
const unsigned long DEBOUNCE_DELAY = 100;     // Button debounce

// ============================================
// SYSTEM STATE ENUM
// ============================================
enum SystemState {
  STATE_WARMUP,
  STATE_READY,
  STATE_SENDING,
  STATE_SHOWING_RESULT,
  STATE_SESSION_CLOSED
};

#endif