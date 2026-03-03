/*
 * SUNSTONE ESP32 - OLED Display Functions
 * File: oled_display.ino (Tab 4)
 * 
 * Semua fungsi untuk display OLED
 */

void setupOLED() {
  Wire.begin(PIN_SDA, PIN_SCL);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("x OLED initialization failed!");
    while (1);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SUNSTONE");
  display.println("Initializing...");
  display.display();
  
  Serial.println("v OLED initialized");
}

// ============================================
// HELPER: Konversi raw ADC ke persen
// ============================================
int toPercent(int rawValue, bool invertedSensor) {
  if (invertedSensor) {
    // MG-811: 4095 = 0%, 0 = 100%
    int pct = (int)(((4095.0 - rawValue) / 4095.0) * 100.0);
    return constrain(pct, 0, 100);
  } else {
    // MQ-3, MQ-7, MQ-135: 0 = 0%, 4095 = 100%
    int pct = (int)((rawValue / 4095.0) * 100.0);
    return constrain(pct, 0, 100);
  }
}

// ============================================
// MAIN UPDATE -- dipanggil dari loop
// ============================================
void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);

  // Hitung nilai sensor persen
  int mq135Pct = toPercent(mq135Value, false);
  int mq3Pct   = toPercent(mq3Value,   false);
  int mq7Pct   = toPercent(mq7Value,   false);
  int mg811Pct = toPercent(mg811Value,  true);  // dibalik!

  // ============================================
  // MODE HASIL (STATE_SHOWING_RESULT)
  // Baris atas  : "Hasil: Tinggi/Sedang/Rendah"
  // Tengah      : nilai sensor persen
  // Baris bawah : "Conf: xx%"
  // ============================================
  if (currentState == STATE_SHOWING_RESULT) {

    // Baris 1: Hasil deteksi (menggantikan WiFi)
    display.setCursor(0, 0);
    display.print("Hasil: ");
    String cat = resultCategory;
    if (cat.length() > 9) cat = cat.substring(0, 9);
    display.print(cat);

    // Separator atas
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Nilai sensor
    display.setCursor(0, 13);
    display.print("MQ-135 : "); display.print(mq135Pct); display.println("%");
    display.setCursor(0, 23);
    display.print("MQ-3   : "); display.print(mq3Pct);   display.println("%");
    display.setCursor(0, 33);
    display.print("MQ-7   : "); display.print(mq7Pct);   display.println("%");
    display.setCursor(0, 43);
    display.print("MG-811 : "); display.print(mg811Pct); display.println("%");

    // Separator bawah
    display.drawLine(0, 54, 127, 54, SSD1306_WHITE);

    // Baris bawah: Confidence (menggantikan sesi)
    display.setCursor(0, 57);
    display.print("Conf: ");
    display.print((int)(resultConfidence * 100));
    display.print("%");

  // ============================================
  // MODE NORMAL (semua state lain)
  // Baris atas  : WiFi
  // Tengah      : nilai sensor persen
  // Baris bawah : nama sesi / "tidak ada"
  // ============================================
  } else {

    // Baris 1: WiFi
    display.setCursor(0, 0);
    display.print("WiFi: ");
    if (WiFi.status() == WL_CONNECTED) {
      String ssidName = WiFi.SSID();
      if (ssidName.length() > 10) ssidName = ssidName.substring(0, 10);
      display.print(ssidName);
    } else {
      display.print("---");
    }

    // Indikator kirim: titik berkedip pojok kanan atas
    if (currentState == STATE_SENDING && buttonIndicator) {
      display.fillCircle(124, 4, 3, SSD1306_WHITE);
    } else if (currentState == STATE_SENDING) {
      display.drawCircle(124, 4, 3, SSD1306_WHITE);
    }

    // Separator atas
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // Nilai sensor
    display.setCursor(0, 13);
    display.print("MQ-135 : "); display.print(mq135Pct); display.println("%");
    display.setCursor(0, 23);
    display.print("MQ-3   : "); display.print(mq3Pct);   display.println("%");
    display.setCursor(0, 33);
    display.print("MQ-7   : "); display.print(mq7Pct);   display.println("%");
    display.setCursor(0, 43);
    display.print("MG-811 : "); display.print(mg811Pct); display.println("%");

    // Separator bawah
    display.drawLine(0, 54, 127, 54, SSD1306_WHITE);

    // Baris bawah: nama sesi
    display.setCursor(0, 57);
    if (hasActiveSession && currentPatientName.length() > 0) {
      display.print("Sesi: ");
      String name = currentPatientName;
      if (name.length() > 10) name = name.substring(0, 10);
      display.print(name);
    } else {
      display.print("Sesi: tidak ada");
    }
  }

  display.display();
}

// ============================================
// FUNGSI PENDUKUNG (dipakai saat init/warmup)
// ============================================
void drawWarmupScreen() {
  unsigned long remaining = WARMUP_DURATION - (millis() - warmupStartTime);
  int seconds = remaining / 1000;
  display.setTextSize(1);
  display.println("Burning sensor:");
  display.println("");
  display.print("Time: ");
  display.print(seconds);
  display.println("s");
}

void drawSessionClosedScreen() {
  display.setTextSize(1);
  display.println("Sesi Selesai");
  display.println("");
  display.println("Tekan tombol utk");
  display.println("cek sesi baru");
}
