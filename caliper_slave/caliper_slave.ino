#include <esp_now.h>
#include <WiFi.h>
#include<Wire.h>
#include<ADXL345_WE.h>
#include "caliper_slave_motor_ctrl.h"
#include "config.h"
#include "common.h"

// #define CLOCK_PIN 23
// #define DATA_PIN 22
// #define TRIG_PIN 21

// Adres MAC mastera
uint8_t masterAddress[] = {0xA0, 0xB7, 0x65, 0x20, 0xC0, 0x8C};

volatile uint8_t bitBuffer[52];
volatile int bitCount = 0;
volatile bool dataReady = false;
volatile bool measurementRequested = false;

Message sensorData;
esp_now_peer_info_t peerInfo;

#define ADXL345_I2CADDR 0x53 // 0x1D if SDO = HIGH
xyzFloat raw, g, angle, corrAngle;
ADXL345_WE myAcc = ADXL345_WE(ADXL345_I2CADDR);

void IRAM_ATTR clockISR() {
  if (bitCount < 52) {
    uint8_t bit = digitalRead(DATA_PIN);
    bitBuffer[bitCount++] = bit;
    if (bitCount == 52) {
      dataReady = true;
    }
  }
}

void reverseBits() {
  for (int i = 0; i < 26; i++) {
    uint8_t temp = bitBuffer[i];
    bitBuffer[i] = bitBuffer[51 - i];
    bitBuffer[51 - i] = temp;
  }
}

float decodeCaliper() {
  uint8_t shifted[52];
  for (int i = 0; i < 52; i++) {
    if (i + 8 < 52) shifted[i] = bitBuffer[i + 8];
    else shifted[i] = 0;
  }
  uint8_t nibbles[13];
  for (int i = 0; i < 13; i++) {
    nibbles[i] = 0;
    for (int j = 0; j < 4; j++)
      nibbles[i] |= (shifted[i * 4 + (3 - j)] << j);
  }
  long value = 0;
  for (int i = 0; i < 5; i++)
    value += nibbles[i] * pow(10, i);
  bool negative = nibbles[6] & 0x08;
  bool inchMode = nibbles[6] & 0x04;
  float measurement = value / 1000.0;
  if (negative) measurement = -measurement;
  if (inchMode) measurement *= 25.4;
  return measurement;
}

// Battery voltage reading with caching and averaging
uint16_t readBatteryVoltage() {
  static uint16_t cachedVoltage = 0;
  static uint32_t lastReadTime = 0;
  
  uint32_t currentTime = millis();
  
  // Return cached value if read interval hasn't passed
  if (cachedVoltage != 0 && (currentTime - lastReadTime) < BATTERY_UPDATE_INTERVAL_MS) {
    return cachedVoltage;
  }
  
  // Read and average multiple samples for better accuracy
  uint32_t adcSum = 0;
  
  for (int i = 0; i < ADC_SAMPLES; i++) {
    adcSum += analogRead(BATTERY_VOLTAGE_PIN);
    delay(1);  // Small delay between samples
  }
  
  int adcAverage = adcSum / ADC_SAMPLES;
  
  // Convert ADC value to millivolts using constants from config.h
  uint16_t voltage_mV = (uint16_t)((adcAverage * ADC_REFERENCE_VOLTAGE_MV) / ADC_RESOLUTION);
  
  // Update cache
  cachedVoltage = voltage_mV;
  lastReadTime = currentTime;
  
  return voltage_mV;
}

float performMeasurement() {
  Serial.println("Wyzwalam pomiar TRIG...");
  digitalWrite(TRIG_PIN, LOW);

  bitCount = 0; dataReady = false;

  attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), clockISR, FALLING);

  unsigned long startTime = millis();
  while (!dataReady && (millis() - startTime < MEASUREMENT_TIMEOUT_MS)) {
    delayMicroseconds(100); // Krótsze opóźnienie dla lepszej responsywności
  }

  detachInterrupt(digitalPinToInterrupt(CLOCK_PIN));
  digitalWrite(TRIG_PIN, HIGH);

  if (dataReady) {
    reverseBits();
    float result = decodeCaliper();

    // Walidacja wyniku z użyciem stałych z config.h
    if (result >= MEASUREMENT_MIN_VALUE && result <= MEASUREMENT_MAX_VALUE && !isnan(result) && !isinf(result)) {
      Serial.print("Pomiar: ");
      Serial.print(result, 3);
      Serial.println(" mm");
      return result;
    } else {
      Serial.println("BLAD: Nieprawidlowa wartosc pomiaru!");
      return INVALID_MEASUREMENT_VALUE;
    }
  } else {
    Serial.println("BŁĄD: Timeout!");
    return INVALID_MEASUREMENT_VALUE;
  }
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len == 1) {
    char command = (char)incomingData[0];
    Serial.print("Otrzymano komendę: ");
    Serial.println(command);
    
    switch (command) {
      case CMD_MEASURE:  // Measurement request
        measurementRequested = true;
        Serial.println("→ Zadanie pomiaru");
        break;
      case CMD_FORWARD:  // Motor forward
        setMotorSpeed(110, MOTOR_FORWARD);
        Serial.println("→ Silnik: Forward");
        break;
      case CMD_REVERSE:  // Motor reverse
        setMotorSpeed(150, MOTOR_REVERSE);
        Serial.println("→ Silnik: Reverse");
        break;
      case CMD_STOP:  // Motor stop
        setMotorSpeed(0, MOTOR_STOP);
        Serial.println("→ Silnik: Stop");
        break;
      default:
        Serial.println("→ Nieznana komenda");
        break;
    }
  } else {
    Serial.println("BLAD: Nieprawidlowe dane ESP-NOW");
  }
}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Status wysyłki: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sukces" : "Błąd");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 SLAVE - Suwmiarka + ESP-NOW ===");
  pinMode(DATA_PIN, INPUT_PULLUP);
  pinMode(CLOCK_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, HIGH);

  Wire.begin();
  if(!myAcc.init()){
    Serial.println("ADXL345 not connected!");
  }

  myAcc.setDataRate(ADXL345_DATA_RATE_50);
  myAcc.setRange(ADXL345_RANGE_2G);

  WiFi.mode(WIFI_STA);
  delay(100);

  int attempts = 0;
  while (WiFi.status() == WL_NO_SHIELD && attempts < 10) {
    delay(100);
    attempts++;
  }

  if (attempts < 10) {
    Serial.print("MAC Address Slave: ");
    Serial.println(WiFi.macAddress());
  } else {
    Serial.println("BŁĄD: WiFi nie może się zainicjalizować!");
    return;
  }

  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    Serial.println("BŁĄD ESP-NOW");
    return;
  }
  Serial.println("ESP-NOW OK");

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;

  int peerTries = 0;
  while (esp_now_add_peer(&peerInfo) != ESP_OK && peerTries < 10) {
    Serial.print("Próba dodania Master... ");
    Serial.println(peerTries + 1);
    delay(100);
    peerTries++;
  }

  if (peerTries < 10) {
    Serial.println("Master dodany jako peer!");
  } else {
    Serial.println("BŁĄD dodania Master! Sprawdź MAC, kanał, zasięg...");
    return;
  }

  Serial.println("Oczekiwanie na żądania pomiaru...\n");
  
  // Initialize motor controller
  Serial.println("Inicjalizacja sterownika silnika...");
  initializeMotorController();
  Serial.println("Sterownik silnika gotowy!\n");
}

void loop() {
  if (measurementRequested) {
    measurementRequested = false;
    float result = performMeasurement();
    sensorData.measurement = result;
    sensorData.valid = (result != INVALID_MEASUREMENT_VALUE);
    sensorData.timestamp = millis();
    sensorData.command = CMD_MEASURE;
    
    // Add battery voltage data
    sensorData.batteryVoltage = readBatteryVoltage();
    myAcc.getRawValues(&raw);
    myAcc.getGValues(&g);
    myAcc.getAngles(&angle);
    myAcc.getCorrAngles(&corrAngle);
    /* Angles use the corrected raws. Angles are simply calculated by
    angle = arcsin(g Value) */
    Serial.print("Angle x  = ");
    Serial.print(angle.x);
    Serial.print("  |  Angle y  = ");
    Serial.print(angle.y);
    Serial.print("  |  Angle z  = ");
    Serial.println(angle.z);

    esp_err_t sendResult = esp_now_send(masterAddress, (uint8_t *) &sensorData, sizeof(sensorData));
    if (sendResult == ESP_OK) {
      Serial.println("Wynik wyslany do Mastera");
      Serial.print("  → Pomiar: ");
      Serial.print(result, 3);
      Serial.print(" mm, Napięcie baterii: ");
      Serial.print(sensorData.batteryVoltage);
      Serial.println("mV");
    } else {
      Serial.print("BLAD wysylania wyniku: ");
      Serial.println(sendResult);

      // Próba ponownego wysłania po krótkiej przerwie
      delay(ESPNOW_RETRY_DELAY_MS);
      sendResult = esp_now_send(masterAddress, (uint8_t *) &sensorData, sizeof(sensorData));
      if (sendResult == ESP_OK) {
        Serial.println("Ponowne wyslanie wyniku udane");
      } else {
        Serial.println("Ponowne wyslanie wyniku nieudane");
      }
    }
    Serial.println("-------------------------------\n");
  }

  // Zamiast delay(10) - lepsza responsywność
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 10) {
    lastCheck = millis();
    // Check motor status periodically
    static int statusCounter = 0;
    if (++statusCounter >= 100) { // Every ~1 second
      statusCounter = 0;
      
      // Send status update with battery voltage
      sensorData.command = CMD_UPDATE;  // Update
      sensorData.batteryVoltage = readBatteryVoltage();
      sensorData.measurement = 0.0;
      sensorData.valid = true;
      sensorData.timestamp = millis();
      
      esp_err_t sendResult = esp_now_send(masterAddress, (uint8_t *) &sensorData, sizeof(sensorData));
      if (sendResult == ESP_OK) {
        Serial.print("Status: Napięcie baterii=");
        Serial.print(sensorData.batteryVoltage);
        Serial.println("mV");
      }

      myAcc.getRawValues(&raw);
      myAcc.getGValues(&g);
      myAcc.getAngles(&angle);
      myAcc.getCorrAngles(&corrAngle);
      /* Angles use the corrected raws. Angles are simply calculated by
      angle = arcsin(g Value) */
      Serial.print("Angle x  = ");
      Serial.print(angle.x);
      Serial.print("  |  Angle y  = ");
      Serial.print(angle.y);
      Serial.print("  |  Angle z  = ");
      Serial.println(angle.z);

    }
  }
}
