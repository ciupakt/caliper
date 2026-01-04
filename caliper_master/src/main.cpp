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
// TODO: Wypisz MAC Address Mastera
WebServer server(WEB_SERVER_PORT);
CommunicationManager commManager;
SystemStatus systemStatus;
PreferencesManager prefsManager;

// Ujednolicona wysyłka: Master → Slave zawsze wysyła pełną strukturę MessageMaster
static constexpr uint8_t DEFAULT_MOTOR_SPEED = 100;
static constexpr uint8_t DEFAULT_MOTOR_TORQUE = 100;
static constexpr MotorState DEFAULT_MOTOR_STATE = MOTOR_STOP;
static constexpr uint32_t DEFAULT_TIMEOUT_MS = 1000;

auto timerWorker = timer_create_default();

// Stan pomiarowy - enkapsulacja zamiast zmiennych globalnych
static MeasurementState measurementState;

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  (void)recv_info;
  MessageSlave msg{};

  if (len != sizeof(msg))
  {
    RECORD_ERROR(ERR_ESPNOW_INVALID_LENGTH, "Received packet length: %d, expected: %d", len, (int)sizeof(msg));
    return;
  }

  memcpy(&msg, incomingData, sizeof(msg));

  systemStatus.msgSlave = msg;
  measurementState.setMeasurement(systemStatus.msgSlave.measurement);
  measurementState.setBatteryVoltage(msg.batteryVoltage);
  measurementState.setReady(true);

  DEBUG_I("ODEBRANO DANE OD SLAVE");
  DEBUG_I("command:%c", (char)systemStatus.msgSlave.command);

  // UI (WWW/GUI) liczy korekcję po swojej stronie:
  // corrected = measurement + calibrationOffset
  DEBUG_PLOT("measurement:%.3f", (double)systemStatus.msgSlave.measurement);
  DEBUG_PLOT("calibrationOffset:%.3f", (double)systemStatus.calibrationOffset);
  DEBUG_PLOT("angleX:%u", (unsigned)msg.angleX);
  DEBUG_PLOT("batteryVoltage:%.3f", (double)msg.batteryVoltage);
  DEBUG_PLOT("sessionName:%s", systemStatus.sessionName);
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

static void initDefaultTxMessage()
{
  memset(&systemStatus.msgMaster, 0, sizeof(systemStatus.msgMaster));
  systemStatus.msgMaster.motorSpeed = DEFAULT_MOTOR_SPEED;
  systemStatus.msgMaster.motorTorque = DEFAULT_MOTOR_TORQUE;
  systemStatus.msgMaster.motorState = DEFAULT_MOTOR_STATE;
  systemStatus.msgMaster.timeout = DEFAULT_TIMEOUT_MS;
}

/**
 * @brief Oblicza timeout oczekiwania na pomiar
 *
 * Funkcja oblicza maksymalny czas oczekiwania na odpowiedź od Slave,
 * dodając margines bezpieczeństwa do timeoutu zdefiniowanego w komendzie.
 *
 * @details
 * - Timeout = msgMaster.timeout + MEASUREMENT_TIMEOUT_MARGIN_MS
 * - Margines MEASUREMENT_TIMEOUT_MARGIN_MS (1000ms) uwzględnia czas:
 *   - transmisji ESP-NOW
 *   - przetwarzania danych po stronie Slave
 *   - opóźnień w komunikacji
 * - W przypadku przepełnienia uint32_t, funkcja zwraca UINT32_MAX
 *
 * @return Timeout w milisekundach (maksymalnie UINT32_MAX)
 */
static uint32_t calcMeasurementWaitTimeoutMs()
{
  // Wymóg: timeout = systemStatus.msgMaster.timeout + MEASUREMENT_TIMEOUT_MARGIN_MS
  // (w razie przepełnienia saturujemy do UINT32_MAX)
  if (systemStatus.msgMaster.timeout > (UINT32_MAX - MEASUREMENT_TIMEOUT_MARGIN_MS))
  {
    return UINT32_MAX;
  }
  return systemStatus.msgMaster.timeout + MEASUREMENT_TIMEOUT_MARGIN_MS;
}

/**
 * @brief Oczekuje na gotowość pomiaru z timeoutem
 *
 * Funkcja blokuje wykonanie programu do momentu otrzymania danych pomiarowych
 * lub upływu timeoutu. Używa flagi measurementReady ustawianej w OnDataRecv.
 *
 * @details
 * - Pętla sprawdza flagę measurementReady co POLL_DELAY_MS (1ms)
 * - Po otrzymaniu danych, funkcja loguje czas oczekiwania
 * - W przypadku timeoutu, funkcja loguje błąd i zwraca false
 * - Funkcja jest blokująca - nie należy jej używać w pętlach czasu rzeczywistego
 *
 * @param timeoutMs Maksymalny czas oczekiwania w milisekundach
 * @return true jeśli dane są gotowe, false w przypadku timeoutu
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

    // Uwaga: blokująca pętla jak wcześniej, ale nie czekamy na sztywne 1000ms.
    delay(POLL_DELAY_MS);
  }

  const uint32_t elapsedMs = millis() - startMs;
  DEBUG_I("Measurement ready after %u ms", (unsigned)elapsedMs);
  return true;
}

ErrorCode sendTxToSlave(CommandType command, const char *commandName, bool expectResponse)
{
  if (expectResponse)
  {
    measurementState.setReady(false);
    measurementState.setMeasurementMessage("Oczekiwanie na odpowiedź...");
  }

  systemStatus.msgMaster.command = command;

  ErrorCode result = commManager.sendMessage(systemStatus.msgMaster);

  if (result == ERR_NONE)
  {
    DEBUG_I("Wyslano komendę: %s", commandName);
    measurementState.setMeasurementMessage(commandName);
  }
  else
  {
    LOG_ERROR(result, "Failed to send command %s", commandName);
    measurementState.setMeasurementMessage("BLAD: Nie można wysłać komendy");
  }

  return result;
}

void requestMeasurement()
{
  (void)sendTxToSlave(CMD_MEASURE, "Pomiar", true);
}

void requestUpdate()
{
  (void)sendTxToSlave(CMD_UPDATE, "Status", true);
}

void sendMotorTest()
{
  (void)sendTxToSlave(CMD_MOTORTEST, "Motor test", false);
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
  server.send(200, "text/plain", "Pomiar wyzwolony");
}

void handleRead()
{
  server.send(200, "text/plain", measurementState.getMeasurement());
}


// --- Kalibracja (WWW)
// 1) POST /api/calibration/measure  -> robi pomiar i zwraca measurementRaw + calibrationOffset
// 2) POST /api/calibration/offset  -> ustawia calibrationOffset (bez wyzwalania pomiaru)

/**
 * @brief Obsługuje żądanie pomiaru kalibracyjnego
 *
 * Endpoint: POST /api/calibration/measure
 *
 * Funkcja wykonuje pomiar i zwraca surową wartość oraz aktualny offset kalibracji.
 *
 * @details
 * Przepływ operacji:
 * 1. Wysyła komendę CMD_MEASURE do Slave
 * 2. Oczekuje na odpowiedź z timeoutem obliczonym przez calcMeasurementWaitTimeoutMs()
 * 3. Jeśli timeout - zwraca błąd 504 Gateway Timeout
 * 4. Jeśli sukces - zwraca JSON z measurementRaw i calibrationOffset
 *
 * Format odpowiedzi JSON:
 * ```json
 * {
 *   "success": true,
 *   "measurementRaw": 123.456,
 *   "calibrationOffset": 0.123
 * }
 * ```
 *
 * Uwaga: UI powinno obliczyć skorygowaną wartość: corrected = measurementRaw + calibrationOffset
 */
void handleCalibrationMeasure()
{
  requestMeasurement();

  // Poczekaj na wynik (MVP: blocking jak dotychczas)
  (void)waitForMeasurementReady(calcMeasurementWaitTimeoutMs());

  if (!measurementState.isReady())
  {
    server.send(504, "application/json", "{\"success\":false,\"error\":\"Brak odpowiedzi z urządzenia\"}");
    return;
  }

  const float raw = systemStatus.msgSlave.measurement;
  const float offset = systemStatus.calibrationOffset;

  char response[JSON_RESPONSE_BUFFER_SIZE];
  snprintf(response, sizeof(response),
    "{\"success\":true,\"measurementRaw\":%.3f,\"calibrationOffset\":%.3f}",
    raw, offset);

  server.send(200, "application/json", response);
}

/**
 * @brief Obsługuje żądanie ustawienia offsetu kalibracji
 *
 * Endpoint: POST /api/calibration/offset
 *
 * Funkcja ustawia offset kalibracji bez wykonywania pomiaru.
 *
 * @details
 * Parametr URL: offset (float) - wartość offsetu w milimetrach
 *
 * Walidacja:
 * - Offset musi być liczbą zmiennoprzecinkową
 * - Zakres: CALIBRATION_OFFSET_MIN (-14.999) do CALIBRATION_OFFSET_MAX (14.999)
 *
 * Przepływ operacji:
 * 1. Pobiera parametr offset z zapytania
 * 2. Waliduje format i zakres wartości
 * 3. Jeśli błąd - zwraca 400 Bad Request
 * 4. Jeśli sukces - zapisuje offset do systemStatus.calibrationOffset
 * 5. Zwraca potwierdzenie z nową wartością
 *
 * Format odpowiedzi JSON:
 * ```json
 * {
 *   "success": true,
 *   "calibrationOffset": 0.123
 * }
 * ```
 *
 * Uwaga: Offset jest przechowywany tylko w pamięci RAM (nie w Preferences),
 * więc zostanie utracony po restarcie urządzenia.
 */
void handleCalibrationSetOffset()
{
  const String offsetStr = server.arg("offset");
  float offsetValue = 0.0f;

  if (!parseFloatStrict(offsetStr, offsetValue))
  {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Niepoprawny parametr offset\"}");
    return;
  }

  if (offsetValue < CALIBRATION_OFFSET_MIN || offsetValue > CALIBRATION_OFFSET_MAX)
  {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Offset poza zakresem (-14.999..14.999)\"}");
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

/**
 * @brief Walidacja nazwy sesji
 *
 * @param name Nazwa sesji do walidacji
 * @return true Nazwa jest prawidłowa
 * @return false Nazwa jest nieprawidłowa
 */
static bool validateSessionName(const String &name)
{
  // Minimalna długość: SESSION_NAME_MIN_LENGTH znak
  if (name.length() < SESSION_NAME_MIN_LENGTH)
  {
    return false;
  }

  // Maksymalna długość: SESSION_NAME_MAX_LENGTH znaków (32 z null terminator)
  if (name.length() > SESSION_NAME_MAX_LENGTH)
  {
    return false;
  }

  // Walidacja znaków: litery (a-z, A-Z), cyfry (0-9), spacje, podkreślenia (_), myślniki (-)
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
  sessionName.replace("%20", " "); // Zamień spacje z URL encoding

  // Walidacja nazwy sesji
  if (!validateSessionName(sessionName))
  {
    server.send(400, "application/json", "{\"error\":\"Nazwa sesji jest nieprawidłowa (maks 31 znaków, dozwolone: a-z, A-Z, 0-9, spacja, _, -)\"}");
    return;
  }

  // Zapisz nazwę sesji do systemStatus.sessionName
  memset(systemStatus.sessionName, 0, sizeof(systemStatus.sessionName));
  strncpy(systemStatus.sessionName, sessionName.c_str(), sizeof(systemStatus.sessionName) - 1);
  
  DEBUG_I("sessionName:%s", systemStatus.sessionName);

  char response[JSON_RESPONSE_BUFFER_SIZE];
  snprintf(response, sizeof(response), "{\"sessionName\":\"%s\"}", sessionName.c_str());
  server.send(200, "application/json", response);
}

/**
 * @brief Obsługuje żądanie pomiaru w ramach aktywnej sesji
 *
 * Endpoint: POST /api/measure_session
 *
 * Funkcja wykonuje pomiar i zwraca wszystkie dane związane z sesją.
 *
 * @details
 * Wymagania:
 * - Sesja musi być aktywna (sessionName nie może być puste)
 * - Nazwa sesji musi być ustawiona przez handleStartSession()
 *
 * Przepływ operacji:
 * 1. Sprawdza czy sesja jest aktywna (sessionName != "")
 * 2. Jeśli nie - zwraca 400 Bad Request
 * 3. Wysyła komendę CMD_MEASURE do Slave
 * 4. Oczekuje na odpowiedź z timeoutem
 * 5. Jeśli timeout - zwraca 504 Gateway Timeout
 * 6. Jeśli sukces - zwraca pełne dane pomiarowe
 *
 * Format odpowiedzi JSON:
 * ```json
 * {
 *   "sessionName": "test_session",
 *   "measurementRaw": 123.456,
 *   "calibrationOffset": 0.123,
 *   "measurementCorrected": 123.579,
 *   "valid": true,
 *   "batteryVoltage": 3.7,
 *   "angleX": 45
 * }
 * ```
 *
 * Pola:
 * - sessionName: nazwa aktywnej sesji
 * - measurementRaw: surowa wartość pomiaru z suwmiarki
 * - calibrationOffset: offset kalibracji
 * - measurementCorrected: skorygowana wartość (raw + offset)
 * - valid: flaga walidacji (zawsze true w tej implementacji)
 * - batteryVoltage: napięcie baterii w woltach
 * - angleX: kąt X z akcelerometru w stopniach
 *
 * Uwaga: measurementCorrected jest obliczane po stronie Mastera
 * dla wygody UI, ale UI może też obliczyć to lokalnie.
 */
void handleMeasureSession()
{
  // Sprawdź czy sesja jest aktywna (sessionName nie jest puste)
  if (strlen(systemStatus.sessionName) == 0)
  {
    server.send(400, "application/json", "{\"error\":\"Sesja nieaktywna (nie ustawiono nazwy sesji)\"}");
    return;
  }

  requestMeasurement();

  // Poczekaj na wynik
  (void)waitForMeasurementReady(calcMeasurementWaitTimeoutMs());

  // Sprawdź czy dane są gotowe przed użyciem
  if (!measurementState.isReady())
  {
    server.send(504, "application/json", "{\"error\":\"Brak odpowiedzi z urządzenia\"}");
    return;
  }

  const MessageSlave &m = systemStatus.msgSlave;

  char response[JSON_RESPONSE_BUFFER_SIZE];
  snprintf(response, sizeof(response),
    "{\"sessionName\":\"%s\",\"measurementRaw\":%.3f,\"calibrationOffset\":%.3f,\"measurementCorrected\":%.3f,\"valid\":true,\"batteryVoltage\":%.3f,\"angleX\":%u}",
    systemStatus.sessionName,
    m.measurement,
    systemStatus.calibrationOffset,
    m.measurement + systemStatus.calibrationOffset,
    m.batteryVoltage,
    (unsigned)m.angleX);

  server.send(200, "application/json", response);
}

void setup()
{
  DEBUG_BEGIN();
  DEBUG_I("=== ESP32 MASTER - Suwmiarka + ESP-NOW ===");

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
    // Load settings from NVS
    prefsManager.loadSettings(&systemStatus);
    
    // Ensure motorState is set to default (not stored in Preferences)
    systemStatus.msgMaster.motorState = DEFAULT_MOTOR_STATE;
    
    // Ensure calibrationOffset is initialized if not loaded
    // (loadSettings handles this with default value)
  }
  
  // sessionName jest już zainicjalizowany na pusty string przez memset

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

  DEBUG_I("\n=== Access Point uruchomiony ===");
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

  // Setup web server routes - static files
  server.on("/", handleRoot);
  server.on("/style.css", handleCSS);
  server.on("/app.js", handleJS);

  // Setup web server routes - API endpoints
  server.on("/measure", handleMeasure);
  server.on("/read", handleRead);

  // Kalibracja
  server.on("/api/calibration/measure", HTTP_POST, handleCalibrationMeasure);
  server.on("/api/calibration/offset", HTTP_POST, handleCalibrationSetOffset);

  server.on("/start_session", HTTP_POST, handleStartSession);
  server.on("/measure_session", HTTP_POST, handleMeasureSession);

  // Handle 404 errors with proper JSON response
  server.onNotFound([]()
                    {
    if (server.method() == HTTP_POST) {
      server.send(404, "application/json", "{\"error\":\"Not found\",\"message\":\"Endpoint nie istnieje\"}");
    } else {
      server.send(404, "text/plain", "Not found");
    } });

  server.begin();
  DEBUG_I("Serwer HTTP uruchomiony na porcie %d", (int)WEB_SERVER_PORT);
  DEBUG_I("Polacz sie z WiFi: %s", WIFI_SSID);
  DEBUG_I("Otworz: http://%s", WiFi.softAPIP().toString().c_str());

  SerialCliContext cliCtx;
  cliCtx.systemStatus = &systemStatus;
  cliCtx.prefsManager = &prefsManager;
  cliCtx.requestMeasurement = requestMeasurement;
  cliCtx.requestUpdate = requestUpdate;
  cliCtx.sendMotorTest = sendMotorTest;
  SerialCli_begin(cliCtx);

  timerWorker.every(200, SerialCli_tick);
}

void loop()
{
  server.handleClient();
  timerWorker.tick();
}
