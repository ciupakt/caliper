#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include "config.h"
#include <shared_common.h>
#include <error_handler.h>
#include <MacroDebugger.h>
#include <espnow_helper.h>
#include <arduino-timer.h>

// Module includes
#include "sensors/caliper.h"
#include "sensors/accelerometer.h"
#include "power/battery.h"
#include "motor/motor_ctrl.h"

// Master device MAC address (defined in config.h)
uint8_t masterAddress[] = MASTER_MAC_ADDR;
// TODO: Wypisz MAC Address Mastera
esp_now_peer_info_t peerInfo;
CaliperInterface caliper;
AccelerometerInterface accelerometer;
BatteryMonitor battery;
MessageMaster msgMaster;
MessageSlave msgSlave;

bool runMeasReq(void *arg);
auto timerWorker = timer_create_default();

/**
 * @brief Callback odbierający dane ESP-NOW od Mastera
 *
 * Funkcja jest wywoływana automatycznie przez ESP-NOW po otrzymaniu
 * pakietu od Mastera. Obsługuje różne komendy i planuje wykonanie
 * odpowiednich akcji za pomocą timera.
 *
 * @details
 * Obsługiwane komendy:
 * - CMD_MEASURE: żądanie pomiaru z uruchomieniem silnika
 * - CMD_UPDATE: żądanie aktualizacji statusu bez silnika
 * - CMD_MOTORTEST: test silnika z parametrami z msgMaster
 *
 * Mechanizm timera:
 * - timerWorker.cancel() anuluje wszystkie zaplanowane zadania
 * - timerWorker.in(TIMER_DELAY_MS, runMeasReq) planuje wykonanie runMeasReq
 * - TIMER_DELAY_MS (1ms) zapewnia minimalne opóźnienie przed wykonaniem
 *
 * Uwaga: Funkcja nie powinna blokować wykonania, dlatego używamy timera
 * do odroczenia wykonania czasochłonnych operacji.
 *
 * @param recv_info Informacje o nadawcy (nieużywane)
 * @param incomingData Bufor z odebranymi danymi
 * @param len Długość odebranych danych
 */
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  (void)recv_info;
  if (len != sizeof(msgMaster))
  {
    RECORD_ERROR(ERR_ESPNOW_INVALID_LENGTH, "Received packet length: %d, expected: %d", len, (int)sizeof(msgMaster));
    return;
  }

  memcpy(&msgMaster, incomingData, sizeof(msgMaster));

  switch (msgMaster.command)
  {
  case CMD_MEASURE: // Measurement request
    DEBUG_I("CMD_MEASURE");
    timerWorker.cancel();
    timerWorker.in(TIMER_DELAY_MS, runMeasReq);
    break;

  case CMD_UPDATE: // Status update request
    DEBUG_I("CMD_UPDATE");
    timerWorker.cancel();
    timerWorker.in(TIMER_DELAY_MS, runMeasReq);
    break;

  // Motor control (generic)
  case CMD_MOTORTEST:
    DEBUG_I("CMD_MOTORTEST");
    motorCtrlRun(msgMaster.motorSpeed, msgMaster.motorTorque, msgMaster.motorState);
    break;

  default:
    DEBUG_W("Nieznana komenda: %c", msgMaster.command);
    break;
  }
}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  (void)info;
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    DEBUG_I("Status wysyłki: Sukces");
  }
  else
  {
    RECORD_ERROR(ERR_ESPNOW_SEND_FAILED, "ESP-NOW send callback reported failure");
  }
}

bool updateMeasureData(void *arg)
{
  accelerometer.update();
  msgSlave.measurement = caliper.performMeasurement();
  msgSlave.angleX = accelerometer.getAngleX();
  msgSlave.batteryVoltage = battery.readVoltageNow();
  msgSlave.command = msgMaster.command;
  return false; // do not repeat this task
}

/**
 * @brief Callback wykonywany przy każdym żądaniu pomiaru
 *
 * Funkcja jest wywoływana przez timer po otrzymaniu komendy CMD_MEASURE
 * lub CMD_UPDATE. Wykonuje pomiar i wysyła wynik do Mastera.
 *
 * @details
 * Przepływ dla CMD_MEASURE:
 * 1. Uruchom silnik w przód (MOTOR_FORWARD) z parametrami z msgMaster
 * 2. Poczekaj msgMaster.timeout ms na ustabilizowanie silnika
 * 3. Wykonaj pomiar (updateMeasureData)
 * 4. Uruchom silnik w tył (MOTOR_REVERSE) do powrotu do pozycji
 * 5. Wyślij wynik do Mastera
 *
 * Przepływ dla CMD_UPDATE:
 * 1. Wykonaj pomiar bez uruchamiania silnika
 * 2. Wyślij wynik do Mastera
 *
 * Mechanizm retry przy błędzie wysyłki:
 * - Pierwsza próba: natychmiast po przygotowaniu danych
 * - Jeśli błąd: odczekaj ESPNOW_RETRY_DELAY_MS (100ms)
 * - Druga próba: ponów wysyłkę
 * - Jeśli drugi błąd: zaloguj błąd i kontynuuj
 *
 * Uwaga: Funkcja zwraca false, co oznacza, że zadanie nie powinno być
 * powtarzane przez timer (jest jednorazowe).
 *
 * @param arg Argument przekazywany przez timer (nieużywany)
 * @return false (zadanie nie jest powtarzane)
 */
bool runMeasReq(void *arg)
{
  if (msgMaster.command == CMD_MEASURE)
  {
    motorCtrlRun(msgMaster.motorSpeed, msgMaster.motorTorque, MOTOR_FORWARD);
    delay(msgMaster.timeout); // wait for motor to stabilize
    DEBUG_I("Czekanie %u ms na ustabilizowanie silnika...", msgMaster.timeout);
    updateMeasureData(nullptr);
    motorCtrlRun(msgMaster.motorSpeed, msgMaster.motorTorque, MOTOR_REVERSE);
    // delay(msgMaster.timeout);
  }
  else if (msgMaster.command == CMD_UPDATE)
  {
    updateMeasureData(nullptr);
  }

  DEBUG_PLOT("measurement:%.3f", msgSlave.measurement);
  DEBUG_PLOT("angleX:%u", msgSlave.angleX);
  DEBUG_PLOT("batteryVoltage:%.3f", msgSlave.batteryVoltage);

  ErrorCode sendResult = espnow_send_with_retry(
      masterAddress,
      &msgSlave,
      sizeof(msgSlave),
      ESPNOW_MAX_RETRIES,
      ESPNOW_RETRY_DELAY_MS
  );

  if (sendResult == ERR_NONE)
  {
    DEBUG_I("Wynik wysłany do Mastera");
  }
  else
  {
    DEBUG_E("Błąd wysyłania wyniku do Mastera");
  }

  return false; // do not repeat this task
}

void setup()
{
  DEBUG_BEGIN();
  DEBUG_I("=== ESP32 SLAVE - Suwmiarka + ESP-NOW ===");

  // Initialize error handler
  ERROR_HANDLER.initialize();

  // Initialize sensors
  caliper.begin();

  Wire.begin();
  if (!accelerometer.begin())
  {
    LOG_WARNING(ERR_ACCEL_INIT_FAILED, "Accelerometer not initialized - continuing without angle data");
  }

  WiFi.mode(WIFI_STA);
  delay(WIFI_INIT_DELAY_MS);

  int attempts = 0;
  while (WiFi.status() == WL_NO_SHIELD && attempts < WIFI_MAX_ATTEMPTS)
  {
    delay(WIFI_RETRY_DELAY_MS);
    attempts++;
  }

  if (attempts < WIFI_MAX_ATTEMPTS)
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
    RECORD_ERROR(ERR_ESPNOW_INIT_FAILED, "ESP-NOW initialization failed");
    return;
  }
  DEBUG_I("ESP-NOW OK");

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;

  ErrorCode peerResult = espnow_add_peer_with_retry(&peerInfo);
  if (peerResult == ERR_NONE)
  {
    DEBUG_I("Master dodany jako peer!");
  }
  else
  {
    DEBUG_E("Nie udało się dodać Mastera jako peer");
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
