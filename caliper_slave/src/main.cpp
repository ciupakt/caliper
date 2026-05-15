#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Preferences.h>
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
#include "ota/ota_update.h"

uint8_t masterAddress[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

Preferences slavePrefs;
esp_now_peer_info_t peerInfo;
CaliperInterface caliper;
AccelerometerInterface accelerometer;
BatteryMonitor battery;
MessageMaster msgMaster;
MessageSlave msgSlave;

volatile bool measurementInProgress = false;

OTAUpdate otaUpdate;
volatile bool otaMode = false;

static bool pairingMode = false;
static uint32_t pairingModeStartMs = 0;
static bool hasStoredMasterMac = false;

bool runMeasReq(void *arg);
bool motorStopTimeout(void *arg);
bool batteryMonitorTask(void *arg);
auto timerWorker = timer_create_default();
auto timerMotorStopTimeout = timer_create_default();
auto timerBattery = timer_create_default();

static bool isMacUnset(const uint8_t mac[6])
{
  for (int i = 0; i < 6; i++)
  {
    if (mac[i] != 0x00) return false;
  }
  return true;
}

static void enterPairingMode()
{
  pairingMode = true;
  pairingModeStartMs = millis();

  esp_now_peer_info_t broadcastPeer{};
  uint8_t broadcastAddr[] = BROADCAST_MAC_ADDR;
  memcpy(broadcastPeer.peer_addr, broadcastAddr, 6);
  broadcastPeer.channel = ESPNOW_WIFI_CHANNEL;
  broadcastPeer.encrypt = false;
  esp_now_add_peer(&broadcastPeer);

  DEBUG_I("Slave: tryb parowania aktywny");
}

static void exitPairingMode()
{
  pairingMode = false;

  uint8_t broadcastAddr[] = BROADCAST_MAC_ADDR;
  esp_now_del_peer(broadcastAddr);

  DEBUG_I("Slave: tryb parowania zakończony");
}

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
 * Mechanizm blokowania pomiarów:
 * - Jeśli measurementInProgress == true, wszystkie komendy są ignorowane
 * - Flaga jest ustawiana na początku runMeasReq i zdejmowana na końcu
 * - Zapobiega to zakłócaniu trwającego pomiaru przez nowe komendy
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
  uint8_t src_addr[6];
  memcpy(src_addr, recv_info->src_addr, 6);

  if (len == sizeof(MessageMaster))
  {
    MessageMaster tmpMsg{};
    memcpy(&tmpMsg, incomingData, sizeof(tmpMsg));

    if (pairingMode && tmpMsg.command == CMD_PAIR)
    {
      if (hasStoredMasterMac)
      {
        esp_now_del_peer(masterAddress);
      }
      memcpy(masterAddress, src_addr, 6);

      memset(&peerInfo, 0, sizeof(peerInfo));
      memcpy(peerInfo.peer_addr, masterAddress, 6);
      peerInfo.channel = ESPNOW_WIFI_CHANNEL;
      peerInfo.encrypt = false;
      espnow_add_peer_with_retry(&peerInfo);

      slavePrefs.putBytes("masterMac", src_addr, 6);
      hasStoredMasterMac = true;

      MessageSlave pairResp{};
      pairResp.command = CMD_PAIR;
      pairResp.measurement = 0;
      pairResp.batteryVoltage = 0;
      pairResp.angleZ = 0;
      espnow_send_with_retry(masterAddress, &pairResp, sizeof(pairResp), ESPNOW_MAX_RETRIES, ESPNOW_RETRY_DELAY_MS);

      exitPairingMode();

      DEBUG_I("Otrzymano CMD_PAIR od Mastera: %02X:%02X:%02X:%02X:%02X:%02X",
        src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5]);
      return;
    }

    if (tmpMsg.command == CMD_PAIR_ACK)
    {
      slavePrefs.putBytes("masterMac", src_addr, 6);
      hasStoredMasterMac = true;
      exitPairingMode();
      DEBUG_I("Parowanie zakończone");
      return;
    }

    memcpy(&msgMaster, &tmpMsg, sizeof(msgMaster));

    if (measurementInProgress)
    {
      DEBUG_W("Pomiar w trakcie - komenda %c zignorowana", msgMaster.command);
      return;
    }

    switch (msgMaster.command)
    {
    case CMD_MEASURE:
      DEBUG_I("CMD_MEASURE");
      timerWorker.cancel();
      timerWorker.in(TIMER_DELAY_MS, runMeasReq);
      break;

    case CMD_UPDATE:
      DEBUG_I("CMD_UPDATE");
      timerWorker.cancel();
      timerWorker.in(TIMER_DELAY_MS, runMeasReq);
      break;

    case CMD_MOTORTEST:
      DEBUG_I("CMD_MOTORTEST");
      motorCtrlRun(msgMaster.motorSpeed, msgMaster.motorTorque, msgMaster.motorState);
      break;

    case CMD_OTA:
      DEBUG_I("CMD_OTA - entering OTA mode");
      otaMode = true;
      break;

    case CMD_PAIR:
    case CMD_PAIR_ACK:
      break;

    default:
      DEBUG_W("Nieznana komenda: %c", msgMaster.command);
      break;
    }
  }
  else
  {
    RECORD_ERROR(ERR_ESPNOW_INVALID_LENGTH, "Received packet length: %d, expected: %d", len, (int)sizeof(MessageMaster));
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
  
  // Pobierz kąt Z - odchylenie od pionu (0-90 stopni)
  float angleZ = accelerometer.getAngleZ();
  msgSlave.angleZ = (uint8_t)angleZ;
  
  msgSlave.batteryVoltage = battery.readVoltageNow();
  msgSlave.command = msgMaster.command;
  return false; // do not repeat this task
}

bool MotorStopTimeout(void *arg)
{
  motorCtrlRun(0, 0, MOTOR_STOP);
  digitalWrite(LED_GREEN, LOW);
  DEBUG_I("Silnik zatrzymany po timeout");
  return false; // do not repeat this task
}

bool batteryMonitorTask(void *arg)
{
  float voltage = battery.readVoltageNow();
  DEBUG_I("Battery: %.0f mV", voltage);

  if (voltage < 7000.0f)
  {
    digitalWrite(LED_RED, HIGH);
  }
  else
  {
    digitalWrite(LED_RED, HIGH);
    delay(1);
    digitalWrite(LED_RED, LOW);
  }
  return true;
}

/**
 * @brief Callback wykonywany przy każdym żądaniu pomiaru
 *
 * Funkcja jest wywoływana przez timer po otrzymaniu komendy CMD_MEASURE
 * lub CMD_UPDATE. Wykonuje pomiar i wysyła wynik do Mastera.
 *
 * @details
 * Przepływ dla CMD_MEASURE:
 * 1. Ustaw flagę measurementInProgress = true (blokuje nowe komendy)
 * 2. Uruchom silnik w przód (MOTOR_FORWARD) z parametrami z msgMaster
 * 3. Poczekaj msgMaster.timeout ms na ustabilizowanie silnika
 * 4. Wykonaj pomiar (updateMeasureData)
 * 5. Uruchom silnik w tył (MOTOR_REVERSE) do powrotu do pozycji
 * 6. Wyślij wynik do Mastera
 * 7. Zdejmij flagę measurementInProgress = false
 *
 * Przepływ dla CMD_UPDATE:
 * 1. Ustaw flagę measurementInProgress = true
 * 2. Wykonaj pomiar bez uruchamiania silnika
 * 3. Wyślij wynik do Mastera
 * 4. Zdejmij flagę measurementInProgress = false
 *
 * Mechanizm blokowania:
 * - Flaga measurementInProgress jest ustawiana na początku funkcji
 * - OnDataRecv sprawdza tę flagę i ignoruje komendy gdy pomiar trwa
 * - Flaga jest zdejmowana po zakończeniu pomiaru i wysyłce wyniku
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
  // Ustaw flagę blokującą nowe komendy
  measurementInProgress = true;
  
  if (msgMaster.command == CMD_MEASURE)
  {
    timerMotorStopTimeout.cancel();

    digitalWrite(LED_GREEN, HIGH);
    motorCtrlRun(msgMaster.motorSpeed, msgMaster.motorTorque, MOTOR_FORWARD);
    DEBUG_I("Czekanie %u ms na ustabilizowanie silnika...", msgMaster.timeout);
    delay(msgMaster.timeout); // wait for motor to stabilize

    digitalWrite(LED_GREEN, LOW);
    updateMeasureData(nullptr);
    digitalWrite(LED_GREEN, HIGH);

    motorCtrlRun(msgMaster.motorSpeed, msgMaster.motorTorque, MOTOR_REVERSE);
    timerMotorStopTimeout.in(msgMaster.timeout, MotorStopTimeout);
  }
  else if (msgMaster.command == CMD_UPDATE)
  {
    updateMeasureData(nullptr);
  }

  DEBUG_PLOT("measurement:%.3f", msgSlave.measurement);
  DEBUG_PLOT("angleZ:%d", msgSlave.angleZ);
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

  // Zdejmij flagę blokującą - pomiar zakończony
  measurementInProgress = false;
  
  return false; // do not repeat this task
}

/**
 * @brief Skanuje magistralę I2C i wyświetla znalezione urządzenia na Serialu
 *
 * Przeszukuje adresy I2C od 0x00 do 0x7F (0-127).
 * Dla każdego adresu próbuje nawiązać transmisję i sprawdza czy urządzenie
 * odpowiada (ACK). Znalezione urządzenia są wyświetlane w formacie hex.
 */
void scanI2C()
{
  DEBUG_I("=== Skanowanie magistrali I2C ===");
  DEBUG_I("Skanowanie adresow 0x00 - 0x7F...");

  uint8_t devicesFound = 0;

  for (uint8_t address = 0x00; address <= 0x7F; address++)
  {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) // Urządzenie odpowiedziało (ACK)
    {
      DEBUG_I("Znaleziono urzadzenie I2C pod adresem: 0x%02X (%d)", address, address);
      devicesFound++;
    }
  }

  if (devicesFound == 0)
  {
    DEBUG_W("Brak urzadzen I2C na magistrali!");
  }
  else
  {
    DEBUG_I("Liczba znalezionych urzadzen I2C: %d", devicesFound);
  }
  DEBUG_I("=== Koniec skanowania I2C ===");
}

void setup()
{
  DEBUG_BEGIN();
  DEBUG_I("=== ESP32 SLAVE - Suwmiarka + ESP-NOW ===");

  ERROR_HANDLER.initialize();

  slavePrefs.begin("caliper_slave", false);

  uint8_t storedMasterMac[6];
  memset(storedMasterMac, 0, 6);
  slavePrefs.getBytes("masterMac", storedMasterMac, 6);

  if (!isMacUnset(storedMasterMac))
  {
    memcpy(masterAddress, storedMasterMac, 6);
    hasStoredMasterMac = true;
    DEBUG_I("Master MAC z NVS: %02X:%02X:%02X:%02X:%02X:%02X",
      masterAddress[0], masterAddress[1], masterAddress[2], masterAddress[3], masterAddress[4], masterAddress[5]);
  }
  else
  {
    DEBUG_I("Brak Master MAC w NVS — oczekiwanie na parowanie");
    hasStoredMasterMac = false;
  }

  caliper.begin();

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

  if (hasStoredMasterMac)
  {
    memcpy(peerInfo.peer_addr, masterAddress, 6);
    peerInfo.channel = ESPNOW_WIFI_CHANNEL;
    peerInfo.encrypt = false;

    ErrorCode peerResult = espnow_add_peer_with_retry(&peerInfo);
    if (peerResult == ERR_NONE)
    {
      DEBUG_I("Master dodany jako peer! MAC: %02X:%02X:%02X:%02X:%02X:%02X",
        masterAddress[0], masterAddress[1], masterAddress[2], masterAddress[3], masterAddress[4], masterAddress[5]);
    }
    else
    {
      DEBUG_E("Nie udało się dodać Mastera jako peer");
      return;
    }
  }
  else
  {
    DEBUG_I("Brak Master MAC — pomijam dodawanie peera, czekam na parowanie");
  }

  DEBUG_I("Inicjalizacja sterownika silnika...");
  motorCtrlInit();
  motorCtrlEnable(true);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  timerBattery.every(BATTERY_UPDATE_INTERVAL_MS, batteryMonitorTask);

  enterPairingMode();

  DEBUG_I("Oczekiwanie na żądania pomiaru...");
}

void loop()
{
  if (otaMode && !otaUpdate.isActive())
  {
    otaUpdate.startOTAMode();
  }
  if (otaUpdate.isActive())
  {
    otaUpdate.handle();
    return;
  }

  if (pairingMode)
  {
    if (!hasStoredMasterMac)
    {
      // Continuous pairing — blink green LED as visual indicator
      uint32_t blink = (millis() / 100) % 2;
      digitalWrite(LED_GREEN, blink ? HIGH : LOW);
    }
    else
    {
      digitalWrite(LED_GREEN, LOW);
      uint32_t elapsed = millis() - pairingModeStartMs;
      if (elapsed >= PAIRING_WINDOW_MS)
      {
        exitPairingMode();
      }
    }
  }

  timerWorker.tick();
  timerMotorStopTimeout.tick();
  timerBattery.tick();
}
