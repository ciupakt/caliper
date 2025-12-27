#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include "config.h"
#include <shared_common.h>

// Module includes
#include "sensors/caliper.h"
#include "sensors/accelerometer.h"
#include "power/battery.h"
#include "motor/motor_ctrl.h"

// Master device MAC address (defined in config.h)
uint8_t masterAddress[] = MASTER_MAC_ADDR;

volatile bool measurementRequested = false;

Message sensorData;
esp_now_peer_info_t peerInfo;

// Sensor instances
CaliperInterface caliper;
AccelerometerInterface accelerometer;
BatteryMonitor battery;

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  if (len == 1)
  {
    char command = (char)incomingData[0];
    Serial.print("Otrzymano komendę: ");
    Serial.println(command);

    switch (command)
    {
    case CMD_MEASURE: // Measurement request
      measurementRequested = true;
      Serial.println("→ Zadanie pomiaru");
      break;
    case CMD_FORWARD: // Motor forward
      setMotorSpeed(110, MOTOR_FORWARD);
      Serial.println("→ Silnik: Forward");
      break;
    case CMD_REVERSE: // Motor reverse
      setMotorSpeed(150, MOTOR_REVERSE);
      Serial.println("→ Silnik: Reverse");
      break;
    case CMD_STOP: // Motor stop
      setMotorSpeed(0, MOTOR_STOP);
      Serial.println("→ Silnik: Stop");
      break;
    default:
      Serial.println("→ Nieznana komenda");
      break;
    }
  }
  else
  {
    Serial.println("BLAD: Nieprawidlowe dane ESP-NOW");
  }
}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  Serial.print("Status wysyłki: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sukces" : "Błąd");
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 SLAVE - Suwmiarka + ESP-NOW ===");

  // Initialize sensors
  caliper.begin();
  
  Wire.begin();
  if (!accelerometer.begin())
  {
    Serial.println("ADXL345 not connected!");
  }

  WiFi.mode(WIFI_STA);
  delay(100);

  int attempts = 0;
  while (WiFi.status() == WL_NO_SHIELD && attempts < 10)
  {
    delay(100);
    attempts++;
  }

  if (attempts < 10)
  {
    Serial.print("MAC Address Slave: ");
    Serial.println(WiFi.macAddress());
  }
  else
  {
    Serial.println("BŁĄD: WiFi nie może się zainicjalizować!");
    return;
  }

  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);

  if (esp_now_init() != ESP_OK)
  {
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
  while (esp_now_add_peer(&peerInfo) != ESP_OK && peerTries < 10)
  {
    Serial.print("Próba dodania Master... ");
    Serial.println(peerTries + 1);
    delay(100);
    peerTries++;
  }

  if (peerTries < 10)
  {
    Serial.println("Master dodany jako peer!");
  }
  else
  {
    Serial.println("BŁĄD dodania Master! Sprawdź MAC, kanał, zasięg...");
    return;
  }

  Serial.println("Oczekiwanie na żądania pomiaru...\n");

  // Initialize motor controller
  Serial.println("Inicjalizacja sterownika silnika...");
  initializeMotorController();
  Serial.println("Sterownik silnika gotowy!\n");
}

void loop()
{
  if (measurementRequested)
  {
    measurementRequested = false;
    float result = caliper.performMeasurement();
    sensorData.measurement = result;
    sensorData.valid = (result != INVALID_MEASUREMENT_VALUE);
    sensorData.timestamp = millis();
    sensorData.command = CMD_MEASURE;

    // Add battery voltage data
    sensorData.batteryVoltage = battery.readVoltage();
    
    // Update accelerometer
    accelerometer.update();
    sensorData.angleX = accelerometer.getAngleX();
    Serial.print(">Angle X:");
    Serial.println(accelerometer.getAngleX());

    esp_err_t sendResult = esp_now_send(masterAddress, (uint8_t *)&sensorData, sizeof(sensorData));
    if (sendResult == ESP_OK)
    {
      Serial.println("Wynik wyslany do Mastera");
      Serial.print("  → Pomiar: ");
      Serial.print(result, 3);
      Serial.print(" mm, Napięcie baterii: ");
      Serial.print(sensorData.batteryVoltage);
      Serial.println("mV");
    }
    else
    {
      Serial.print("BLAD wysylania wyniku: ");
      Serial.println(sendResult);

      // Próba ponownego wysłania po krótkiej przerwie
      delay(ESPNOW_RETRY_DELAY_MS);
      sendResult = esp_now_send(masterAddress, (uint8_t *)&sensorData, sizeof(sensorData));
      if (sendResult == ESP_OK)
      {
        Serial.println("Ponowne wyslanie wyniku udane");
      }
      else
      {
        Serial.println("Ponowne wyslanie wyniku nieudane");
      }
    }
    Serial.println("-------------------------------\n");
  }

  // Zamiast delay(10) - lepsza responsywność
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 10)
  {
    lastCheck = millis();
    // Check motor status periodically
    static int statusCounter = 0;
    if (++statusCounter >= 100)
    { // Every ~1 second
      statusCounter = 0;

      // Send status update with battery voltage
      sensorData.command = CMD_UPDATE; // Update
      sensorData.batteryVoltage = battery.readVoltage();
      sensorData.measurement = 0.0;
      sensorData.valid = true;
      sensorData.timestamp = millis();

      // Update accelerometer
      accelerometer.update();
      sensorData.angleX = accelerometer.getAngleX();

      esp_err_t sendResult = esp_now_send(masterAddress, (uint8_t *)&sensorData, sizeof(sensorData));
      if (sendResult == ESP_OK)
      {
        Serial.print(">Status: Napięcie baterii:");
        Serial.print(sensorData.batteryVoltage);
        Serial.println(" mV");
      }

      Serial.print(">Angle X:");
      Serial.println(accelerometer.getAngleX());
    }
  }
}
