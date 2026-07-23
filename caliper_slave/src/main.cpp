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

  DEBUG_I("Slave: pairing mode active");
}

static void exitPairingMode()
{
  pairingMode = false;

  uint8_t broadcastAddr[] = BROADCAST_MAC_ADDR;
  esp_now_del_peer(broadcastAddr);

  DEBUG_I("Slave: pairing mode ended");
}

/**
 * @brief ESP-NOW data receive callback from Master
 *
 * This function is called automatically by ESP-NOW upon receiving
 * a packet from Master. It handles various commands and schedules
 * appropriate actions using a timer.
 *
 * @details
 * Supported commands:
 * - CMD_MEASURE: measurement request with motor activation
 * - CMD_UPDATE: status update request without motor
 * - CMD_MOTORTEST: motor test with parameters from msgMaster
 *
 * Measurement locking mechanism:
 * - If measurementInProgress == true, all commands are ignored
 * - The flag is set at the start of runMeasReq and cleared at the end
 * - This prevents ongoing measurements from being disrupted by new commands
 *
 * Timer mechanism:
 * - timerWorker.cancel() cancels all scheduled tasks
 * - timerWorker.in(TIMER_DELAY_MS, runMeasReq) schedules runMeasReq for execution
 * - TIMER_DELAY_MS (1ms) provides minimal delay before execution
 *
 * Note: This function should not block execution, so we use a timer
 * to defer execution of time-consuming operations.
 *
 * @param recv_info Sender information (unused)
 * @param incomingData Buffer with received data
 * @param len Length of received data
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

      DEBUG_I("Received CMD_PAIR from Master: %02X:%02X:%02X:%02X:%02X:%02X",
        src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5]);
      return;
    }

    if (tmpMsg.command == CMD_PAIR_ACK)
    {
      slavePrefs.putBytes("masterMac", src_addr, 6);
      hasStoredMasterMac = true;
      exitPairingMode();
      DEBUG_I("Pairing completed");
      return;
    }

    memcpy(&msgMaster, &tmpMsg, sizeof(msgMaster));

    if (measurementInProgress)
    {
      DEBUG_W("Measurement in progress - command %c ignored", msgMaster.command);
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
      DEBUG_W("Unknown command: %c", msgMaster.command);
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
    DEBUG_I("Send status: Success");
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
  
  // Get Z angle - vertical deviation (0-90 degrees)
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
  DEBUG_I("Motor stopped after timeout");
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
 * @brief Callback executed on each measurement request
 *
 * This function is called by the timer after receiving CMD_MEASURE
 * or CMD_UPDATE. Performs measurement and sends result to Master.
 *
 * @details
 * Flow for CMD_MEASURE:
 * 1. Set measurementInProgress = true (blocks new commands)
 * 2. Start motor forward (MOTOR_FORWARD) with parameters from msgMaster
 * 3. Wait msgMaster.timeout ms for motor stabilization
 * 4. Perform measurement (updateMeasureData)
 * 5. Start motor reverse (MOTOR_REVERSE) to return to position
 * 6. Send result to Master
 * 7. Clear measurementInProgress = false
 *
 * Flow for CMD_UPDATE:
 * 1. Set measurementInProgress = true
 * 2. Perform measurement without activating motor
 * 3. Send result to Master
 * 4. Clear measurementInProgress = false
 *
 * Locking mechanism:
 * - The measurementInProgress flag is set at the start of the function
 * - OnDataRecv checks this flag and ignores commands when measurement is in progress
 * - The flag is cleared after measurement completes and result is sent
 *
 * Retry mechanism on send error:
 * - First attempt: immediately after data is prepared
 * - On error: wait ESPNOW_RETRY_DELAY_MS (100ms)
 * - Second attempt: retry send
 * - On second error: log error and continue
 *
 * Note: This function returns false, meaning the task should not be
 * repeated by timer (it is one-shot).
 *
 * @param arg Argument passed by timer (unused)
 * @return false (task is not repeated)
 */
bool runMeasReq(void *arg)
{
  // Set flag blocking new commands
  measurementInProgress = true;
  
  if (msgMaster.command == CMD_MEASURE)
  {
    timerMotorStopTimeout.cancel();

    digitalWrite(LED_GREEN, HIGH);
    motorCtrlRun(msgMaster.motorSpeed, msgMaster.motorTorque, MOTOR_FORWARD);
    DEBUG_I("Waiting %u ms for motor stabilization...", msgMaster.timeout);
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
    DEBUG_I("Result sent to Master");
  }
  else
  {
    DEBUG_E("Error sending result to Master");
  }

  // Clear blocking flag - measurement completed
  measurementInProgress = false;
  
  return false; // do not repeat this task
}

/**
 * @brief Scans I2C bus and displays found devices on Serial
 *
 * Scans I2C addresses from 0x00 to 0x7F (0-127).
 * For each address, attempts to initiate transmission and checks if device
 * responds (ACK). Found devices are displayed in hex format.
 */
void scanI2C()
{
  DEBUG_I("=== Scanning I2C bus ===");
  DEBUG_I("Scanning addresses 0x00 - 0x7F...");

  uint8_t devicesFound = 0;

  for (uint8_t address = 0x00; address <= 0x7F; address++)
  {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) // Device responded (ACK)
    {
      DEBUG_I("Found I2C device at address: 0x%02X (%d)", address, address);
      devicesFound++;
    }
  }

  if (devicesFound == 0)
  {
    DEBUG_W("No I2C devices found on the bus!");
  }
  else
  {
    DEBUG_I("Number of found I2C devices: %d", devicesFound);
  }
  DEBUG_I("=== End of I2C scan ===");
}

void setup()
{
  DEBUG_BEGIN();
  DEBUG_I("=== ESP32 SLAVE - Caliper + ESP-NOW ===");

  ERROR_HANDLER.initialize();

  slavePrefs.begin("caliper_slave", false);

  uint8_t storedMasterMac[6];
  memset(storedMasterMac, 0, 6);
  slavePrefs.getBytes("masterMac", storedMasterMac, 6);

  if (!isMacUnset(storedMasterMac))
  {
    memcpy(masterAddress, storedMasterMac, 6);
    hasStoredMasterMac = true;
    DEBUG_I("Master MAC from NVS: %02X:%02X:%02X:%02X:%02X:%02X",
      masterAddress[0], masterAddress[1], masterAddress[2], masterAddress[3], masterAddress[4], masterAddress[5]);
  }
  else
  {
    DEBUG_I("No Master MAC in NVS — waiting for pairing");
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
    DEBUG_E("ERROR: WiFi cannot initialize!");
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
      DEBUG_I("Master added as peer! MAC: %02X:%02X:%02X:%02X:%02X:%02X",
        masterAddress[0], masterAddress[1], masterAddress[2], masterAddress[3], masterAddress[4], masterAddress[5]);
    }
    else
    {
      DEBUG_E("Failed to add Master as peer");
      return;
    }
  }
  else
  {
    DEBUG_I("No Master MAC — skipping peer addition, waiting for pairing");
  }

  DEBUG_I("Initializing motor controller...");
  motorCtrlInit();
  motorCtrlEnable(true);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  timerBattery.every(BATTERY_UPDATE_INTERVAL_MS, batteryMonitorTask);

  enterPairingMode();

  DEBUG_I("Waiting for measurement requests...");
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
