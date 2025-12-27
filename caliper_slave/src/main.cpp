#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include "config.h"
#include <shared_common.h>
#include <MacroDebugger.h>
#include <arduino-timer.h>

// Module includes
#include "sensors/caliper.h"
#include "sensors/accelerometer.h"
#include "power/battery.h"
#include "motor/motor_ctrl.h"

// Master device MAC address (defined in config.h)
uint8_t masterAddress[] = MASTER_MAC_ADDR;
Message sensorData;
esp_now_peer_info_t peerInfo;

// Sensor instances
CaliperInterface caliper;
AccelerometerInterface accelerometer;
BatteryMonitor battery;

bool measReq(void *);
bool updateStatus(void *);

auto timerUpdateStatus = timer_create_default();
auto timerMeasReq = timer_create_default();

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  if (len == 1)
  {
    char command = (char)incomingData[0];
    DEBUG_I("Otrzymano komendę: %c", command);

    switch (command)
    {
    case CMD_MEASURE: // Measurement request
      DEBUG_I("Zadanie pomiaru");
      timerMeasReq.cancel();
      timerMeasReq.in(1, measReq);
      break;
    case CMD_FORWARD: // Motor forward
      DEBUG_I("Silnik: Forward");
      setMotorSpeed(110, MOTOR_FORWARD);
      break;
    case CMD_REVERSE: // Motor reverse
      DEBUG_I("Silnik: Reverse");
      setMotorSpeed(150, MOTOR_REVERSE);
      break;
    case CMD_STOP: // Motor stop
      DEBUG_I("Silnik: Stop");
      setMotorSpeed(0, MOTOR_STOP);
      break;
    default:
      DEBUG_W("Nieznana komenda: %c", command);
      break;
    }
  }
  else
  {
    DEBUG_E("BŁĄD: Nieprawidłowe dane ESP-NOW (len=%d)", len);
  }
}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    DEBUG_I("Status wysyłki: Sukces");
  }
  else
  {
    DEBUG_W("Status wysyłki: Błąd");
  }
}

// callback evry measurement request
bool measReq(void *)
{
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

  esp_err_t sendResult = esp_now_send(masterAddress, (uint8_t *)&sensorData, sizeof(sensorData));
  if (sendResult == ESP_OK)
  {
    DEBUG_I("Wynik wysłany do Mastera");
    DEBUG_PLOT("measurement:%.3f", (unsigned)sensorData.measurement);
    DEBUG_PLOT("timestamp:%u", (unsigned)sensorData.timestamp);
    DEBUG_PLOT("batteryVoltage:%u", (unsigned)sensorData.batteryVoltage);
    DEBUG_PLOT("angleX:%u", (unsigned)sensorData.angleX);
  }
  else
  {
    DEBUG_E("BŁĄD wysyłania wyniku: %d", (int)sendResult);

    // Próba ponownego wysłania po krótkiej przerwie
    delay(ESPNOW_RETRY_DELAY_MS);
    sendResult = esp_now_send(masterAddress, (uint8_t *)&sensorData, sizeof(sensorData));
    if (sendResult == ESP_OK)
    {
      DEBUG_I("Ponowne wysłanie wyniku udane");
    }
    else
    {
      DEBUG_E("Ponowne wysłanie wyniku nieudane: %d", (int)sendResult);
    }
  }

  return false; // do not repeat this task
}

// callback every status update interval
bool updateStatus(void *)
{
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
  if (sendResult != ESP_OK)
  {
    DEBUG_E("sendResult: %d", sendResult);
  }

  DEBUG_PLOT("batteryVoltage:%u", (unsigned)sensorData.batteryVoltage);
  DEBUG_PLOT("angleX:%u", (unsigned)sensorData.angleX);

  return true; // repeat this task
}

void setup()
{
  DEBUG_BEGIN();
  delay(1000);
  DEBUG_I("=== ESP32 SLAVE - Suwmiarka + ESP-NOW ===");

  // Initialize sensors
  caliper.begin();

  Wire.begin();
  if (!accelerometer.begin())
  {
    DEBUG_W("Akcelerometr (ADXL345) nie został zainicjalizowany");
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
    DEBUG_I("MAC Address Slave: %s", WiFi.macAddress().c_str());
  }
  else
  {
    DEBUG_E("BŁĄD: WiFi nie może się zainicjalizować!");
    return;
  }

  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);

  if (esp_now_init() != ESP_OK)
  {
    DEBUG_E("BŁĄD ESP-NOW");
    return;
  }
  DEBUG_I("ESP-NOW OK");

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;

  int peerTries = 0;
  while (esp_now_add_peer(&peerInfo) != ESP_OK && peerTries < 10)
  {
    DEBUG_I("Próba dodania Master... %d", peerTries + 1);
    delay(100);
    peerTries++;
  }

  if (peerTries < 10)
  {
    DEBUG_I("Master dodany jako peer!");
  }
  else
  {
    DEBUG_E("BŁĄD dodania Master! Sprawdź MAC, kanał, zasięg...");
    return;
  }

  // Initialize motor controller
  DEBUG_I("Inicjalizacja sterownika silnika...");
  initializeMotorController();
  DEBUG_I("Oczekiwanie na żądania pomiaru...");
  timerUpdateStatus.every(1000, updateStatus);
}

void loop()
{
  timerUpdateStatus.tick();
  timerMeasReq.tick();
}
