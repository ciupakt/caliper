#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"
#include <shared_common.h>
#include <MacroDebugger.h>
#include <arduino-timer.h>
#include "communication.h"
#include "serial_cli.h"
#include "preferences_manager.h"

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

// Global variables
// Offset kalibracji (mm) jest utrzymywany w systemStatus.calibrationOffset
String lastMeasurement = "Brak pomiaru";
String lastBatteryVoltage = "Brak danych";
float lastMeasurementValue = 0.0f; // Ostatnie wartości liczbowe (ułatwia logowanie / JSON)
bool measurementReady = false;

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  (void)recv_info;
  MessageSlave msg{};

  if (len != sizeof(msg))
  {
    DEBUG_E("BLAD: Nieprawidlowa dlugosc pakietu ESP-NOW");
    return;
  }

  memcpy(&msg, incomingData, sizeof(msg));

  systemStatus.msgSlave = msg;
  lastMeasurementValue = systemStatus.msgSlave.measurement;

  DEBUG_I("ODEBRANO DANE OD SLAVE");
  DEBUG_I("command:%c", (char)systemStatus.msgSlave.command);

  // UI (WWW/GUI) liczy korekcję po swojej stronie:
  // corrected = measurement + calibrationOffset
  DEBUG_PLOT("measurement:%.3f", (double)systemStatus.msgSlave.measurement);
  DEBUG_PLOT("calibrationOffset:%.3f", (double)systemStatus.calibrationOffset);
  DEBUG_PLOT("angleX:%u", (unsigned)msg.angleX);
  DEBUG_PLOT("batteryVoltage:%.3f", (double)msg.batteryVoltage);
  DEBUG_PLOT("sessionName:%s", systemStatus.sessionName);

  lastMeasurement = String(systemStatus.msgSlave.measurement, 3) + " mm";
  lastBatteryVoltage = String(msg.batteryVoltage, 3) + " V";

  measurementReady = true;
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

static void initDefaultTxMessage()
{
  memset(&systemStatus.msgMaster, 0, sizeof(systemStatus.msgMaster));
  systemStatus.msgMaster.motorSpeed = DEFAULT_MOTOR_SPEED;
  systemStatus.msgMaster.motorTorque = DEFAULT_MOTOR_TORQUE;
  systemStatus.msgMaster.motorState = DEFAULT_MOTOR_STATE;
  systemStatus.msgMaster.timeout = DEFAULT_TIMEOUT_MS;
}

static uint32_t calcMeasurementWaitTimeoutMs()
{
  // Wymóg: timeout = systemStatus.msgMaster.timeout + 1000ms
  // (w razie przepełnienia saturujemy do UINT32_MAX)
  if (systemStatus.msgMaster.timeout > (UINT32_MAX - 1000U))
  {
    return UINT32_MAX;
  }
  return systemStatus.msgMaster.timeout + 1000U;
}

static bool waitForMeasurementReady(uint32_t timeoutMs)
{
  const uint32_t startMs = millis();

  while (!measurementReady)
  {
    const uint32_t elapsedMs = millis() - startMs;
    if (elapsedMs >= timeoutMs)
    {
      DEBUG_W("Measurement timeout after %u ms (limit=%u ms)", (unsigned)elapsedMs, (unsigned)timeoutMs);
      return false;
    }

    // Uwaga: blokująca pętla jak wcześniej, ale nie czekamy na sztywne 1000ms.
    delay(1);
  }

  const uint32_t elapsedMs = millis() - startMs;
  DEBUG_I("Measurement ready after %u ms", (unsigned)elapsedMs);
  return true;
}

ErrorCode sendTxToSlave(CommandType command, const char *commandName, bool expectResponse)
{
  if (expectResponse)
  {
    measurementReady = false;
    lastMeasurement = "Oczekiwanie na odpowiedź...";
  }

  systemStatus.msgMaster.command = command;

  ErrorCode result = commManager.sendMessage(systemStatus.msgMaster);

  if (result == ERR_NONE)
  {
    DEBUG_I("Wyslano komendę: %s", commandName);
    lastMeasurement = String("Komenda: ") + commandName;
  }
  else
  {
    DEBUG_E("BLAD wysylania komendy %s: %d", commandName, (int)result);
    lastMeasurement = "BLAD: Nie można wysłać komendy";
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
  server.send(200, "text/plain", lastMeasurement);
}


// --- Kalibracja (WWW)
// 1) POST /api/calibration/measure  -> robi pomiar i zwraca measurementRaw + calibrationOffset
// 2) POST /api/calibration/offset  -> ustawia calibrationOffset (bez wyzwalania pomiaru)

void handleCalibrationMeasure()
{
  requestMeasurement();

  // Poczekaj na wynik (MVP: blocking jak dotychczas)
  (void)waitForMeasurementReady(calcMeasurementWaitTimeoutMs());

  if (!measurementReady)
  {
    server.send(504, "application/json", "{\"success\":false,\"error\":\"Brak odpowiedzi z urządzenia\"}");
    return;
  }

  const float raw = systemStatus.msgSlave.measurement;
  const float offset = systemStatus.calibrationOffset;

  String response = "{";
  response += "\"success\":true,";
  response += "\"measurementRaw\":" + String((double)raw, 3) + ",";
  response += "\"calibrationOffset\":" + String((double)offset, 3);
  response += "}";

  server.send(200, "application/json", response);
}

void handleCalibrationSetOffset()
{
  const String offsetStr = server.arg("offset");
  float offsetValue = 0.0f;

  if (!parseFloatStrict(offsetStr, offsetValue))
  {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Niepoprawny parametr offset\"}");
    return;
  }

  if (offsetValue < -14.999f || offsetValue > 14.999f)
  {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Offset poza zakresem (-14.999..14.999)\"}");
    return;
  }

  systemStatus.calibrationOffset = offsetValue;
  DEBUG_I("calibrationOffset:%.3f", (double)systemStatus.calibrationOffset);

  String response = "{";
  response += "\"success\":true,";
  response += "\"calibrationOffset\":" + String((double)systemStatus.calibrationOffset, 3);
  response += "}";

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
  // Minimalna długość: 1 znak
  if (name.length() < 1)
  {
    return false;
  }

  // Maksymalna długość: 31 znaków (32 z null terminator)
  if (name.length() > 31)
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

  String response = "{\"sessionName\":\"" + sessionName + "\"}";
  server.send(200, "application/json", response);
}

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
  if (!measurementReady)
  {
    server.send(504, "application/json", "{\"error\":\"Brak odpowiedzi z urządzenia\"}");
    return;
  }

  const MessageSlave &m = systemStatus.msgSlave;

  String response = "{";
  response += "\"sessionName\":\"" + String(systemStatus.sessionName) + "\",";

  response += "\"measurementRaw\":" + String((double)m.measurement, 3) + ",";
  response += "\"calibrationOffset\":" + String((double)systemStatus.calibrationOffset, 3) + ",";

  // (opcjonalne) pole pomocnicze dla UI/debug
  response += "\"measurementCorrected\":" + String((double)(m.measurement + systemStatus.calibrationOffset), 3) + ",";

  response += "\"valid\":true,";
  response += "\"batteryVoltage\":" + String(m.batteryVoltage, 3) + ",";
  response += "\"angleX\":" + String((unsigned)m.angleX);
  response += "}";

  server.send(200, "application/json", response);
}

void setup()
{
  DEBUG_BEGIN();
  DEBUG_I("=== ESP32 MASTER - Suwmiarka + ESP-NOW ===");

  // Initialize system status
  memset(&systemStatus, 0, sizeof(systemStatus));
  
  // Initialize Preferences Manager and load settings
  if (!prefsManager.begin())
  {
    DEBUG_W("PreferencesManager initialization failed, using default values");
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
    DEBUG_E("LittleFS Mount Failed!");
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
  if (commManager.initialize(slaveAddress) != ERR_NONE)
  {
    DEBUG_E("ESP-NOW init failed!");
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
