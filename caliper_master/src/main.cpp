#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"
#include <shared_common.h>
#include <error_handler.h>
#include <MacroDebugger.h>
#include <arduino-timer.h>
#include "communication.h"
#include "serial_cli.h"
#include "preferences_manager.h"
#include "measurement_state.h"

// Slave device MAC address (defined in config.h)
uint8_t slaveAddress[] = SLAVE_MAC_ADDR;
uint8_t rcAddress[] = RC_MAC_ADDR;

static bool pairingMode = false;
static uint32_t pairingModeStartMs = 0;
static uint32_t lastPairBroadcastMs = 0;
static uint8_t pairedRcAddress[6] = {};
static bool hasPairedRc = false;
static volatile bool rcTrigMeasPending = false;
static volatile bool rcDropMeasPending = false;
// TODO: Print Master MAC Address
WebServer server(WEB_SERVER_PORT);
CommunicationManager commManager;
SystemStatus systemStatus;
PreferencesManager prefsManager;

// Unified sending: Master → Slave always sends the full MessageMaster structure
static constexpr uint8_t DEFAULT_MOTOR_SPEED = 100;
static constexpr uint8_t DEFAULT_MOTOR_TORQUE = 100;
static constexpr MotorState DEFAULT_MOTOR_STATE = MOTOR_STOP;
static constexpr uint32_t DEFAULT_TIMEOUT_MS = 1000;

auto timerWorker = timer_create_default();

// Measurement state - encapsulation instead of global variables
static MeasurementState measurementState;

static void requestMeasurement();

static void enterPairingMode()
{
  pairingMode = true;
  pairingModeStartMs = millis();
  lastPairBroadcastMs = 0;

  esp_now_peer_info_t broadcastPeer{};
  uint8_t broadcastAddr[] = BROADCAST_MAC_ADDR;
  memcpy(broadcastPeer.peer_addr, broadcastAddr, 6);
  broadcastPeer.channel = ESPNOW_WIFI_CHANNEL;
  broadcastPeer.encrypt = false;
  esp_now_add_peer(&broadcastPeer);

  DEBUG_I("Pairing mode active (10s)");
  DEBUG_PLOT("pairing:1");
}

static void exitPairingMode()
{
  pairingMode = false;

  uint8_t broadcastAddr[] = BROADCAST_MAC_ADDR;
  esp_now_del_peer(broadcastAddr);

  DEBUG_I("Pairing mode ended");
  DEBUG_PLOT("pairing:0");
}

static bool isMacUnset(const uint8_t mac[6])
{
  for (int i = 0; i < 6; i++)
  {
    if (mac[i] != 0x00) return false;
  }
  return true;
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  uint8_t src_addr[6];
  memcpy(src_addr, recv_info->src_addr, 6);

  if (len == sizeof(MessageSlave))
  {
    MessageSlave msg{};
    memcpy(&msg, incomingData, sizeof(msg));

  if (rcTrigMeasPending)
  {
    rcTrigMeasPending = false;
    DEBUG_I("RC command: R");
    requestMeasurement();
  }

  if (rcDropMeasPending)
  {
    rcDropMeasPending = false;
    DEBUG_I("RC: DROP_MEAS -> dropMeas:1");
    DEBUG_PLOT("dropMeas:1");
  }

  if (pairingMode)
    {
      commManager.updatePeerAddress(src_addr);
      prefsManager.saveSlaveMac(src_addr);
      memcpy(slaveAddress, src_addr, 6);

      systemStatus.msgMaster.command = CMD_PAIR_ACK;
      commManager.sendMessage(systemStatus.msgMaster);

      DEBUG_I("New Slave paired: %02X:%02X:%02X:%02X:%02X:%02X",
        src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5]);
    }

    systemStatus.msgSlave = msg;
    measurementState.setMeasurement(systemStatus.msgSlave.measurement);
    measurementState.setBatteryVoltage(msg.batteryVoltage);
    measurementState.setReady(true);
  }
  else if (len == sizeof(MessageRC))
  {
    MessageRC msg{};
    memcpy(&msg, incomingData, sizeof(msg));

    if (pairingMode && msg.command == CMD_PAIR)
    {
      memcpy(pairedRcAddress, src_addr, 6);
      hasPairedRc = true;

      esp_now_del_peer(src_addr);
      commManager.addRcPeer(src_addr);

      prefsManager.saveRcMac(src_addr);

      MessageMaster ackMsg{};
      ackMsg.command = CMD_PAIR_ACK;
      commManager.sendMessage(ackMsg);

      DEBUG_I("New RC paired: %02X:%02X:%02X:%02X:%02X:%02X",
        src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5]);
      return;
    }

    if (msg.command == CMD_TRIG_MEAS)
    {
      rcTrigMeasPending = true;
    }
    else if (msg.command == CMD_DROP_MEAS)
    {
      rcDropMeasPending = true;
    }
    else if (msg.command != CMD_PAIR && msg.command != CMD_PAIR_ACK)
    {
      DEBUG_W("RC: unknown command: %c", (char)msg.command);
    }
  }
  else
  {
    RECORD_ERROR(ERR_ESPNOW_INVALID_LENGTH, "Received packet length: %d, expected: %d (Slave) or %d (RC)", len, (int)sizeof(MessageSlave), (int)sizeof(MessageRC));
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

static void initDefaultTxMessage()
{
  memset(&systemStatus.msgMaster, 0, sizeof(systemStatus.msgMaster));
  systemStatus.msgMaster.motorSpeed = DEFAULT_MOTOR_SPEED;
  systemStatus.msgMaster.motorTorque = DEFAULT_MOTOR_TORQUE;
  systemStatus.msgMaster.motorState = DEFAULT_MOTOR_STATE;
  systemStatus.msgMaster.timeout = DEFAULT_TIMEOUT_MS;
}

/**
 * @brief Calculates the measurement wait timeout
 *
 * Calculates the maximum wait time for a response from Slave,
 * adding a safety margin to the timeout defined in the command.
 *
 * @details
 * - Timeout = msgMaster.timeout + MEASUREMENT_TIMEOUT_MARGIN_MS
 * - The MEASUREMENT_TIMEOUT_MARGIN_MS (1000ms) margin accounts for:
 *   - ESP-NOW transmission
 *   - data processing on the Slave side
 *   - communication delays
 * - In case of uint32_t overflow, the function returns UINT32_MAX
 *
 * @return Timeout in milliseconds (maximum UINT32_MAX)
 */
static uint32_t calcMeasurementWaitTimeoutMs()
{
  // Requirement: timeout = systemStatus.msgMaster.timeout + MEASUREMENT_TIMEOUT_MARGIN_MS
  // (in case of overflow, saturate to UINT32_MAX)
  if (systemStatus.msgMaster.timeout > (UINT32_MAX - MEASUREMENT_TIMEOUT_MARGIN_MS))
  {
    return UINT32_MAX;
  }
  return systemStatus.msgMaster.timeout + MEASUREMENT_TIMEOUT_MARGIN_MS;
}

/**
 * @brief Waits for measurement readiness with timeout
 *
 * This function blocks program execution until measurement data is received
 * or timeout expiration. Uses the measurementReady flag set in OnDataRecv.
 *
 * @details
 * - The loop checks the measurementReady flag every POLL_DELAY_MS (1ms)
 * - After receiving data, the function logs the wait time
 * - On timeout, the function logs an error and returns false
 * - This function is blocking - do not use in real-time loops
 *
 * @param timeoutMs Maximum wait time in milliseconds
 * @return true if data is ready, false on timeout
 */
static bool waitForMeasurementReady(uint32_t timeoutMs)
{
  const uint32_t startMs = millis();

  while (!measurementState.isReady())
  {
    const uint32_t elapsedMs = millis() - startMs;
    if (elapsedMs >= timeoutMs)
    {
      DEBUG_W("Measurement timeout after %u ms (limit=%u ms)", (unsigned)elapsedMs, (unsigned)timeoutMs);
      return false;
    }

    // Note: blocking loop as before, but we don't wait for a fixed 1000ms.
    delay(POLL_DELAY_MS);
  }

  const uint32_t elapsedMs = millis() - startMs;
  DEBUG_I("Measurement ready after %u ms", (unsigned)elapsedMs);
  DEBUG_I("command:%c", (char)systemStatus.msgSlave.command);

  // UI (Web/GUI) calculates correction on its side:
  // corrected = measurement + calibrationOffset
  DEBUG_PLOT("sessionName:%s", systemStatus.sessionName);
  DEBUG_PLOT("calibrationOffset:%.3f", (double)systemStatus.calibrationOffset);
  DEBUG_PLOT("reference:%.3f", (double)systemStatus.reference);
  DEBUG_PLOT("angleZ:%u", (unsigned)systemStatus.msgSlave.angleZ);
  DEBUG_PLOT("measurement:%.3f", (double)systemStatus.msgSlave.measurement);
  DEBUG_PLOT("batteryVoltage:%.3f", (double)systemStatus.msgSlave.batteryVoltage);

  return true;
}

/**
 * @brief Executes a measurement operation with race condition protection
 *
 * This function ensures atomic execution of CMD_MEASURE or CMD_UPDATE
 * with protection against simultaneous calls from different sources.
 *
 * @details
 * Operation flow:
 * 1. Checks if an operation is already in progress (measurementInProgress)
 * 2. If yes - returns false (error: busy)
 * 3. If no - sets measurementInProgress = true
 * 4. Resets the ready flag
 * 5. Sends command to Slave
 * 6. Waits for response with timeout
 * 7. Sets measurementInProgress = false
 * 8. Returns true (success) or false (timeout/error)
 *
 * @param command Command type (CMD_MEASURE or CMD_UPDATE)
 * @param commandName Command name for logging
 * @return true if the operation succeeded, false otherwise
 */
static bool executeMeasurementCommand(CommandType command, const char *commandName)
{
  // Step 1: Check if operation is already in progress
  if (measurementState.isMeasurementInProgress())
  {
    DEBUG_W("Measurement command %s rejected - operation already in progress", commandName);
    return false;
  }

  // Step 2: Lock the operation
  measurementState.setMeasurementInProgress(true);

  // Step 3: Reset the ready flag
  measurementState.setReady(false);
  measurementState.setMeasurementMessage("Waiting for response...");

  // Step 4: Set and send the command
  systemStatus.msgMaster.command = command;

  ErrorCode result = commManager.sendMessage(systemStatus.msgMaster);

  if (result != ERR_NONE)
  {
    LOG_ERROR(result, "Failed to send command %s", commandName);
    measurementState.setMeasurementMessage("ERROR: Cannot send command");
    measurementState.setMeasurementInProgress(false);  // Release lock
    return false;
  }

  DEBUG_I("Command sent: %s", commandName);
  measurementState.setMeasurementMessage(commandName);

  // Step 5: Wait for response
  bool success = waitForMeasurementReady(calcMeasurementWaitTimeoutMs());

  // Step 6: Release lock (even on timeout)
  measurementState.setMeasurementInProgress(false);

  return success;
}

ErrorCode sendTxToSlave(CommandType command, const char *commandName, bool expectResponse)
{
  if (expectResponse)
  {
    measurementState.setReady(false);
    measurementState.setMeasurementMessage("Waiting for response...");
  }

  systemStatus.msgMaster.command = command;

  ErrorCode result = commManager.sendMessage(systemStatus.msgMaster);

  if (result == ERR_NONE)
  {
    DEBUG_I("Command sent: %s", commandName);
    measurementState.setMeasurementMessage(commandName);
  }
  else
  {
    LOG_ERROR(result, "Failed to send command %s", commandName);
    measurementState.setMeasurementMessage("ERROR: Cannot send command");
  }

  return result;
}

void requestMeasurement()
{
  executeMeasurementCommand(CMD_MEASURE, "Measure");
}

void requestUpdate()
{
  executeMeasurementCommand(CMD_UPDATE, "Update");
}

void sendMotorTest()
{
  (void)sendTxToSlave(CMD_MOTORTEST, "Motor test", false);
}

void sendOTA()
{
  (void)sendTxToSlave(CMD_OTA, "OTA update", false);
}

// Serve static files from LittleFS
void handleRoot()
{
  File file = LittleFS.open("/index.html", "r");
  if (!file)
  {
    server.send(500, "text/plain", "Failed to open index.html");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleCSS()
{
  File file = LittleFS.open("/style.css", "r");
  if (!file)
  {
    server.send(404, "text/plain", "CSS file not found");
    return;
  }
  server.streamFile(file, "text/css");
  file.close();
}

void handleJS()
{
  File file = LittleFS.open("/app.js", "r");
  if (!file)
  {
    server.send(404, "text/plain", "JS file not found");
    return;
  }
  server.streamFile(file, "application/javascript");
  file.close();
}

void handleMeasure()
{
  requestMeasurement();
  server.send(200, "text/plain", "Measurement triggered");
}

void handleRead()
{
  server.send(200, "text/plain", measurementState.getMeasurement());
}


// --- Calibration (Web)
// 1) POST /api/calibration/measure  -> performs measurement and returns measurementRaw + calibrationOffset
// 2) POST /api/calibration/offset  -> sets calibrationOffset (without triggering measurement)

/**
 * @brief Handles calibration measurement request
 *
 * Endpoint: POST /api/calibration/measure
 *
 * Performs a measurement and returns the raw value and current calibration offset.
 *
 * @details
 * Operation flow:
 * 1. Sends CMD_MEASURE to Slave (with race condition protection)
 * 2. If operation is already in progress - returns 503 Service Unavailable
 * 3. If timeout - returns 504 Gateway Timeout
 * 4. If success - returns JSON with measurementRaw and calibrationOffset
 *
 * JSON response format:
 * ```json
 * {
 *   "success": true,
 *   "measurementRaw": 123.456,
 *   "calibrationOffset": 0.123
 * }
 * ```
 *
 * Note: UI should calculate the corrected value: corrected = measurementRaw + calibrationOffset
 */
void handleCalibrationMeasure()
{
  // Call unified function with race condition protection
  if (!executeMeasurementCommand(CMD_MEASURE, "Measure"))
  {
    // Check whether it's a timeout or busy state
    if (!measurementState.isReady())
    {
      server.send(504, "application/json", "{\"success\":false,\"error\":\"No response from device\"}");
    }
    else
    {
      server.send(503, "application/json", "{\"success\":false,\"error\":\"Device busy - operation in progress\"}");
    }
    return;
  }

  const float raw = systemStatus.msgSlave.measurement;
  const float offset = systemStatus.calibrationOffset;
  const float ref = systemStatus.reference;

  char response[JSON_RESPONSE_BUFFER_SIZE];
  snprintf(response, sizeof(response),
    "{\"success\":true,\"measurementRaw\":%.3f,\"calibrationOffset\":%.3f,\"reference\":%.3f}",
    raw, offset, ref);

  server.send(200, "application/json", response);
}

/**
 * @brief Handles calibration offset set request
 *
 * Endpoint: POST /api/calibration/offset
 *
 * This function sets the calibration offset without performing a measurement.
 *
 * @details
 * URL parameter: offset (float) - offset value in millimeters
 *
 * Validation:
 * - Offset must be a floating-point number
 * - Range: CALIBRATION_OFFSET_MIN (-14.999) to CALIBRATION_OFFSET_MAX (14.999)
 *
 * Operation flow:
 * 1. Gets the offset parameter from the request
 * 2. Validates format and value range
 * 3. On error - returns 400 Bad Request
 * 4. On success - saves offset to systemStatus.calibrationOffset
 * 5. Returns confirmation with the new value
 *
 * JSON response format:
 * ```json
 * {
 *   "success": true,
 *   "calibrationOffset": 0.123
 * }
 * ```
 *
 * Note: Offset is stored only in RAM (not in Preferences),
 * so it will be lost after device restart.
 */
void handleCalibrationSetOffset()
{
  const String offsetStr = server.arg("offset");
  float offsetValue = 0.0f;

  if (!parseFloatStrict(offsetStr, offsetValue))
  {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid offset parameter\"}");
    return;
  }

  if (offsetValue < CALIBRATION_OFFSET_MIN || offsetValue > CALIBRATION_OFFSET_MAX)
  {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Offset out of range (-14.999..14.999)\"}");
    return;
  }

  systemStatus.calibrationOffset = offsetValue;
  DEBUG_I("calibrationOffset:%.3f", (double)systemStatus.calibrationOffset);

  char response[JSON_RESPONSE_BUFFER_SIZE];
  snprintf(response, sizeof(response),
    "{\"success\":true,\"calibrationOffset\":%.3f}",
    systemStatus.calibrationOffset);

  server.send(200, "application/json", response);
}

void handleReferenceSet()
{
  const String refStr = server.arg("reference");
  float refValue = 0.0f;

  if (!parseFloatStrict(refStr, refValue))
  {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid reference parameter\"}");
    return;
  }

  if (refValue < REFERENCE_MIN || refValue > REFERENCE_MAX)
  {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Reference out of range (-999.999..999.999)\"}");
    return;
  }

  systemStatus.reference = refValue;
  DEBUG_I("reference:%.3f", (double)systemStatus.reference);

  char response[JSON_RESPONSE_BUFFER_SIZE];
  snprintf(response, sizeof(response),
    "{\"success\":true,\"reference\":%.3f}",
    systemStatus.reference);

  server.send(200, "application/json", response);
}

/**
 * @brief Validates session name
 *
 * @param name Session name to validate
 * @return true Name is valid
 * @return false Name is invalid
 */
static bool validateSessionName(const String &name)
{
  // Minimum length: SESSION_NAME_MIN_LENGTH character
  if (name.length() < SESSION_NAME_MIN_LENGTH)
  {
    return false;
  }

  // Maximum length: SESSION_NAME_MAX_LENGTH characters (32 with null terminator)
  if (name.length() > SESSION_NAME_MAX_LENGTH)
  {
    return false;
  }

  // Character validation: letters (a-z, A-Z), digits (0-9), spaces, underscores (_), hyphens (-)
  for (unsigned int i = 0; i < name.length(); i++)
  {
    char c = name.charAt(i);
    if (!(isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-'))
    {
      return false;
    }
  }

  return true;
}

void handleStartSession()
{
  String sessionName = server.arg("sessionName");
  sessionName.replace("%20", " "); // Replace spaces from URL encoding

  // Validate session name
  if (!validateSessionName(sessionName))
  {
    server.send(400, "application/json", "{\"error\":\"Session name is invalid (max 31 characters, allowed: a-z, A-Z, 0-9, space, _, -)\"}");
    return;
  }

  // Save session name to systemStatus.sessionName
  memset(systemStatus.sessionName, 0, sizeof(systemStatus.sessionName));
  strncpy(systemStatus.sessionName, sessionName.c_str(), sizeof(systemStatus.sessionName) - 1);
  
  DEBUG_PLOT("sessionName:%s", systemStatus.sessionName);

  char response[JSON_RESPONSE_BUFFER_SIZE];
  snprintf(response, sizeof(response), "{\"sessionName\":\"%s\"}", sessionName.c_str());
  server.send(200, "application/json", response);
}

/**
 * @brief Handles measurement request within an active session
 *
 * Endpoint: POST /api/measure_session
 *
 * Performs a measurement and returns all session-related data.
 *
 * @details
 * Requirements:
 * - Session must be active (sessionName must not be empty)
 * - Session name must be set via handleStartSession()
 *
 * Operation flow:
 * 1. Checks if session is active (sessionName != "")
 * 2. If not - returns 400 Bad Request
 * 3. Sends CMD_MEASURE to Slave (with race condition protection)
 * 4. If operation is already in progress - returns 503 Service Unavailable
 * 5. If timeout - returns 504 Gateway Timeout
 * 6. On success - returns full measurement data
 *
 * JSON response format:
 * ```json
 * {
 *   "sessionName": "test_session",
 *   "measurementRaw": 123.456,
 *   "calibrationOffset": 0.123,
 *   "measurementCorrected": 123.579,
 *   "valid": true,
 *   "batteryVoltage": 3.7,
 *   "angleZ": 45
 * }
 * ```
 *
 * Fields:
 * - sessionName: name of the active session
 * - measurementRaw: raw measurement value from caliper
 * - calibrationOffset: calibration offset
 * - measurementCorrected: corrected value (raw + offset)
 * - valid: validation flag (always true in this implementation)
 * - batteryVoltage: battery voltage in volts
 * - angleZ: vertical deviation from accelerometer in degrees (0-90°)
 *
 * Note: measurementCorrected is calculated on the Master side
 * for UI convenience, but UI can also calculate it locally.
 */
void handleMeasureSession()
{
  // Check if session is active (sessionName is not empty)
  if (strlen(systemStatus.sessionName) == 0)
  {
    server.send(400, "application/json", "{\"error\":\"Session inactive (session name not set)\"}");
    return;
  }

  // Call unified function with race condition protection
  if (!executeMeasurementCommand(CMD_MEASURE, "Measure"))
  {
    // Check whether it's a timeout or busy state
    if (!measurementState.isReady())
    {
      server.send(504, "application/json", "{\"error\":\"No response from device\"}");
    }
    else
    {
      server.send(503, "application/json", "{\"error\":\"Device busy - operation in progress\"}");
    }
    return;
  }

  const MessageSlave &m = systemStatus.msgSlave;

  char response[JSON_RESPONSE_BUFFER_SIZE];
  snprintf(response, sizeof(response),
    "{\"sessionName\":\"%s\",\"measurementRaw\":%.3f,\"calibrationOffset\":%.3f,\"reference\":%.3f,\"measurementCorrected\":%.3f,\"valid\":true,\"batteryVoltage\":%.3f,\"angleZ\":%u}",
    systemStatus.sessionName,
    m.measurement,
    systemStatus.calibrationOffset,
    systemStatus.reference,
    m.measurement + systemStatus.calibrationOffset + systemStatus.reference,
    m.batteryVoltage,
    (unsigned)m.angleZ);

  server.send(200, "application/json", response);
}

void setup()
{
  DEBUG_BEGIN();
  DEBUG_I("=== ESP32 MASTER - Caliper + ESP-NOW ===");

  // Initialize system status
  memset(&systemStatus, 0, sizeof(systemStatus));
  
  // Initialize error handler
  ERROR_HANDLER.initialize();
  
  // Initialize Preferences Manager and load settings
  if (!prefsManager.begin())
  {
    RECORD_ERROR(ERR_PREFS_INIT_FAILED, "PreferencesManager initialization failed, using default values");
    initDefaultTxMessage();
  }
  else
  {
    prefsManager.loadSettings(&systemStatus);
    
    systemStatus.msgMaster.motorState = DEFAULT_MOTOR_STATE;

    uint8_t nvsSlaveMac[6];
    if (prefsManager.loadSlaveMac(nvsSlaveMac))
    {
      memcpy(slaveAddress, nvsSlaveMac, 6);
      DEBUG_I("Slave MAC from NVS: %02X:%02X:%02X:%02X:%02X:%02X",
        slaveAddress[0], slaveAddress[1], slaveAddress[2], slaveAddress[3], slaveAddress[4], slaveAddress[5]);
    }
    else
    {
      DEBUG_W("Slave MAC not found in NVS — using fallback from config.h");
    }

    uint8_t nvsRcMac[6];
    if (prefsManager.loadRcMac(nvsRcMac))
    {
      memcpy(rcAddress, nvsRcMac, 6);
      DEBUG_I("RC MAC from NVS: %02X:%02X:%02X:%02X:%02X:%02X",
        rcAddress[0], rcAddress[1], rcAddress[2], rcAddress[3], rcAddress[4], rcAddress[5]);
    }
    else
    {
      DEBUG_W("RC MAC not found in NVS — using fallback from config.h");
    }
  }
  
  // sessionName is already initialized to empty string by memset

  // Initialize LittleFS
  if (!LittleFS.begin())
  {
    RECORD_ERROR(ERR_LITTLEFS_MOUNT_FAILED, "Failed to mount LittleFS file system");
    return;
  }
  DEBUG_I("LittleFS mounted successfully");

  // Setup WiFi
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  DEBUG_I("\n=== Access Point started ===");
  DEBUG_I("SSID: %s", WIFI_SSID);
  DEBUG_I("IP: %s", WiFi.softAPIP().toString().c_str());
  DEBUG_I("================================\n");
  DEBUG_I("MAC Address Master: %s", WiFi.macAddress().c_str());
  DEBUG_I("");

  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);

  // Initialize communication manager
  ErrorCode commResult = commManager.initialize(slaveAddress);
  if (commResult != ERR_NONE)
  {
    LOG_ERROR(commResult, "Failed to initialize ESP-NOW communication");
    return;
  }

  // Set callbacks
  commManager.setReceiveCallback(OnDataRecv);
  commManager.setSendCallback(OnDataSent);

  // Add RC device as ESP-NOW peer (only if MAC is not unset)
  if (!isMacUnset(rcAddress))
  {
    esp_now_peer_info_t rcPeerInfo{};
    memcpy(rcPeerInfo.peer_addr, rcAddress, 6);
    rcPeerInfo.channel = ESPNOW_WIFI_CHANNEL;
    rcPeerInfo.encrypt = false;
    if (esp_now_add_peer(&rcPeerInfo) == ESP_OK)
    {
      DEBUG_I("RC peer added: %02X:%02X:%02X:%02X:%02X:%02X",
        rcAddress[0], rcAddress[1], rcAddress[2], rcAddress[3], rcAddress[4], rcAddress[5]);
      memcpy(pairedRcAddress, rcAddress, 6);
      hasPairedRc = true;
    }
    else
    {
      DEBUG_W("Failed to add RC peer");
    }
  }
  else
  {
    DEBUG_I("RC MAC unset — RC peer will not be added (use pairing)");
  }

  // Setup web server routes - static files
  server.on("/", handleRoot);
  server.on("/style.css", handleCSS);
  server.on("/app.js", handleJS);

  // Setup web server routes - API endpoints
  server.on("/measure", handleMeasure);
  server.on("/read", handleRead);

  // Calibration
  server.on("/api/calibration/measure", HTTP_POST, handleCalibrationMeasure);
  server.on("/api/calibration/offset", HTTP_POST, handleCalibrationSetOffset);
  server.on("/api/reference", HTTP_POST, handleReferenceSet);

  server.on("/start_session", HTTP_POST, handleStartSession);
  server.on("/measure_session", HTTP_POST, handleMeasureSession);
  // Handle 404 errors with proper JSON response
  server.onNotFound([]()
                    {
    if (server.method() == HTTP_POST) {
      server.send(404, "application/json", "{\"error\":\"Not found\",\"message\":\"Endpoint not found\"}");
    } else {
      server.send(404, "text/plain", "Not found");
    } });

  server.begin();
  DEBUG_I("HTTP server started on port %d", (int)WEB_SERVER_PORT);
  DEBUG_I("Connect to WiFi: %s", WIFI_SSID);
  DEBUG_I("Open: http://%s", WiFi.softAPIP().toString().c_str());

  SerialCliContext cliCtx;
  cliCtx.systemStatus = &systemStatus;
  cliCtx.prefsManager = &prefsManager;
  cliCtx.measurementState = &measurementState;
  cliCtx.requestMeasurement = requestMeasurement;
  cliCtx.requestUpdate = requestUpdate;
  cliCtx.sendMotorTest = sendMotorTest;
  cliCtx.sendOTA = sendOTA;
  cliCtx.enterPairingMode = enterPairingMode;
  SerialCli_begin(cliCtx);

  timerWorker.every(200, SerialCli_tick);
}

void loop()
{
  if (rcTrigMeasPending)
  {
    rcTrigMeasPending = false;
    DEBUG_I("RC command: R");
    requestMeasurement();
  }

  if (rcDropMeasPending)
  {
    rcDropMeasPending = false;
    DEBUG_I("RC: DROP_MEAS -> dropMeas:1");
    DEBUG_PLOT("dropMeas:1");
  }

  if (pairingMode)
  {
    uint32_t now = millis();
    uint32_t elapsed = now - pairingModeStartMs;

    if (elapsed >= PAIRING_MODE_TIMEOUT_MS)
    {
      exitPairingMode();
    }
    else
    {
      static uint8_t lastCountdownSec = 255;
      uint8_t remainingSec = (PAIRING_MODE_TIMEOUT_MS - elapsed) / 1000;
      if (remainingSec != lastCountdownSec)
      {
        lastCountdownSec = remainingSec;
        DEBUG_PLOT("pairingCountdown:%u", (unsigned)remainingSec);
      }

      if (now - lastPairBroadcastMs >= PAIRING_BROADCAST_INTERVAL_MS)
      {
        lastPairBroadcastMs = now;
        MessageMaster pairMsg{};
        pairMsg.command = CMD_PAIR;
        uint8_t broadcastAddr[] = BROADCAST_MAC_ADDR;
        esp_now_send(broadcastAddr, (const uint8_t *)&pairMsg, sizeof(pairMsg));
      }
    }
  }

  server.handleClient();
  timerWorker.tick();
}
