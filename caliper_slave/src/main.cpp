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
//TODO: Wypisz MAC Address Mastera
esp_now_peer_info_t peerInfo;
CaliperInterface caliper;
AccelerometerInterface accelerometer;
BatteryMonitor battery;
MessageMaster msgSlave;
MessageSlave msgMaster;

bool runMeasReq(void *arg);
auto timerWorker = timer_create_default();

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  if (len != sizeof(msgSlave))
  {
    DEBUG_E("BLAD: Nieprawidlowa dlugosc pakietu ESP-NOW");
    return;
  }

  memcpy(&msgSlave, incomingData, sizeof(msgSlave));

  switch (msgSlave.command)
  {
  case CMD_MEASURE: // Measurement request
    DEBUG_I("CMD_MEASURE");
    timerWorker.cancel();
    timerWorker.in(1, runMeasReq);
    break;

  case CMD_UPDATE: // Status update request
    DEBUG_I("CMD_UPDATE");
    timerWorker.cancel();
    timerWorker.in(1, runMeasReq);
    break;

  // Motor control (generic)
  case CMD_MOTORTEST:
    DEBUG_I("CMD_MOTORTEST");
    motorCtrlRun(msgSlave.motorSpeed, msgSlave.motorTorque, msgSlave.motorState);
    break;

  default:
    DEBUG_W("Nieznana komenda: %c", msgSlave.command);
    break;
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
bool runMeasReq(void *arg)
{
  accelerometer.update();
  msgMaster.measurement = caliper.performMeasurement();
  msgMaster.angleX = accelerometer.getAngleX();
  msgMaster.batteryVoltage = battery.readVoltageNow();
  msgMaster.timestamp = millis();
  msgMaster.command = msgSlave.command;

  DEBUG_I("command:%c", msgMaster.command);

  DEBUG_PLOT("timestamp:%u", msgMaster.timestamp);
  DEBUG_PLOT("measurement:%.3f", msgMaster.measurement);
  DEBUG_PLOT("angleX:%u", msgMaster.angleX);
  DEBUG_PLOT("batteryVoltage:%.3f", msgMaster.batteryVoltage);

  esp_err_t sendResult = esp_now_send(masterAddress, (uint8_t *)&msgMaster, sizeof(msgMaster));
  if (sendResult == ESP_OK)
  {
    DEBUG_I("Wynik wysłany do Mastera");
  }
  else
  {
    DEBUG_E("BŁĄD wysyłania wyniku: %d", (int)sendResult);

    // Próba ponownego wysłania po krótkiej przerwie
    delay(ESPNOW_RETRY_DELAY_MS);
    sendResult = esp_now_send(masterAddress, (uint8_t *)&msgMaster, sizeof(msgMaster));
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


void setup()
{
  DEBUG_BEGIN();
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
  motorCtrlInit();
  DEBUG_I("Oczekiwanie na żądania pomiaru...");
}

void loop()
{
  timerWorker.tick();
}
