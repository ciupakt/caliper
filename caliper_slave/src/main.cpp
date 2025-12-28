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
Message msg;

bool runMeasReq(void *arg);
auto timerWorker = timer_create_default();

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  if (len != sizeof(msg))
  {
    DEBUG_E("BLAD: Nieprawidlowa dlugosc pakietu ESP-NOW");
    return;
  }

  memcpy(&msg, incomingData, sizeof(msg));
  // CMD_MEASURE = 'M', /**< Request measurement from slave */
  // CMD_UPDATE = 'U',  /**< Request update status from slave */
  // CMD_FORWARD = 'F', /**< Motor forward command */
  // CMD_REVERSE = 'R', /**< Motor reverse command */
  // CMD_STOP = 'S',    /**< Motor stop command */

  switch (msg.command)
  {
  case CMD_MEASURE: // Measurement request
    DEBUG_I("Zadanie pomiaru");
    timerWorker.cancel();
    timerWorker.in(1, runMeasReq);
    break;
  case CMD_UPDATE: // Status update request
    DEBUG_I("Aktualizacja statusu");
    timerWorker.cancel();
    timerWorker.in(1, runMeasReq);
    break;
  case CMD_FORWARD: // Motor forward
    DEBUG_I("Silnik: Forward");
    motorCtrlRun(msg.motorSpeed, msg.motorTorque, MOTOR_FORWARD);
    break;
  case CMD_REVERSE: // Motor reverse
    DEBUG_I("Silnik: Reverse");
    motorCtrlRun(msg.motorSpeed, msg.motorTorque, MOTOR_REVERSE);
    break;
  case CMD_STOP: // Motor stop
    DEBUG_I("Silnik: Stop");
    motorCtrlRun(0, 0, MOTOR_STOP);
    break;
  default:
    DEBUG_W("Nieznana komenda: %c", msg.command);
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
  msg.measurement = caliper.performMeasurement();
  msg.angleX = accelerometer.getAngleX();
  msg.batteryVoltage = battery.readVoltage();
  msg.timestamp = millis();

  (void)msg.command;     // Already known;
  (void)msg.motorSpeed;  // Not used in measurement response
  (void)msg.motorTorque; // Not used in measurement response

  DEBUG_I("command:%c", msg.command);
  DEBUG_I("motorSpeed:%u", (unsigned)msg.motorSpeed);
  DEBUG_I("motorTorque:%u", (unsigned)msg.motorTorque);

  DEBUG_PLOT("timestamp:%u", (unsigned)msg.timestamp);
  DEBUG_PLOT("measurement:%.3f", (unsigned)msg.measurement);
  DEBUG_PLOT("angleX:%u", (unsigned)msg.angleX);
  DEBUG_PLOT("batteryVoltage:%u", (unsigned)msg.batteryVoltage);

  esp_err_t sendResult = esp_now_send(masterAddress, (uint8_t *)&msg, sizeof(msg));
  if (sendResult == ESP_OK)
  {
    DEBUG_I("Wynik wysłany do Mastera");
  }
  else
  {
    DEBUG_E("BŁĄD wysyłania wyniku: %d", (int)sendResult);

    // Próba ponownego wysłania po krótkiej przerwie
    delay(ESPNOW_RETRY_DELAY_MS);
    sendResult = esp_now_send(masterAddress, (uint8_t *)&msg, sizeof(msg));
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
