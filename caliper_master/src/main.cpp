#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"
#include <shared_common.h>
#include <MacroDebugger.h>
#include <arduino-timer.h>
#include "communication.h"

// Slave device MAC address (defined in config.h)
uint8_t slaveAddress[] = SLAVE_MAC_ADDR;
// TODO: Wypisz MAC Address Mastera
WebServer server(WEB_SERVER_PORT);
CommunicationManager commManager;
SystemStatus systemStatus;
bool serialCommandParser(void *arg);
static bool parseIntStrict(const String &s, long &out);
static bool parseFloatStrict(const String &s, float &out);

auto timerWorker = timer_create_default();

// Global variables
String lastMeasurement = "Brak pomiaru";
String lastBatteryVoltage = "Brak danych";
bool measurementReady = false;

// Ostatnie wartości liczbowe (ułatwia logowanie / JSON / kalibrację)
float lastMeasurementValue = 0.0f;
float lastDeviationValue = 0.0f;

// Session management variables
String currentSessionName = "";
bool sessionActive = false;
float calibrationError = 0.0f;
// Offset kalibracji (mm) jest utrzymywany w systemStatus.localCalibrationOffset

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
  measurementReady = true;

  systemStatus.msgSlave = msg;
  systemStatus.localDeviation = systemStatus.msgSlave.measurement + systemStatus.localCalibrationOffset;

  lastMeasurementValue = systemStatus.msgSlave.measurement;
  lastDeviationValue = systemStatus.localDeviation;

  DEBUG_I("command:%c", (char)msg.command);
  DEBUG_I("measurement:%.3f", (double)msg.measurement);
  DEBUG_I("timestamp:%u", (unsigned)msg.timestamp);

  DEBUG_PLOT("localDeviation:%.3f", (double)systemStatus.localDeviation);
  DEBUG_PLOT("angleX:%u", (unsigned)msg.angleX);
  DEBUG_PLOT("batteryVoltage:%.3f", (double)msg.batteryVoltage);

  lastMeasurement = String(systemStatus.localDeviation, 3) + " mm";
  lastBatteryVoltage = String(msg.batteryVoltage, 3) + " V";
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

// Ujednolicona wysyłka: Master → Slave zawsze wysyła pełną strukturę MessageMaster
static constexpr uint8_t DEFAULT_MOTOR_SPEED = 255;
static constexpr uint8_t DEFAULT_MOTOR_TORQUE = 0;
static constexpr MotorState DEFAULT_MOTOR_STATE = MOTOR_STOP;
static constexpr uint32_t DEFAULT_TIMEOUT_MS = 0;

static void initDefaultTxMessage()
{
  memset(&systemStatus.msgMaster, 0, sizeof(systemStatus.msgMaster));
  systemStatus.msgMaster.motorSpeed = DEFAULT_MOTOR_SPEED;
  systemStatus.msgMaster.motorTorque = DEFAULT_MOTOR_TORQUE;
  systemStatus.msgMaster.motorState = DEFAULT_MOTOR_STATE;
  systemStatus.msgMaster.timeout = DEFAULT_TIMEOUT_MS;
}

ErrorCode sendTxToSlave(CommandType command, const char *commandName, bool expectResponse)
{
  if (expectResponse)
  {
    measurementReady = false;
    lastMeasurement = "Oczekiwanie na odpowiedź...";
  }

  systemStatus.msgMaster.command = command;
  systemStatus.msgMaster.timestamp = millis();

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

void requestCalibration()
{
  DEBUG_I("Rozpoczynanie kalibracji z offsetem: %.3f", (double)systemStatus.localCalibrationOffset);
  DEBUG_I("Rozpoczynanie kalibracji z offsetem: %.3f", (double)systemStatus.localCalibrationOffset);
  (void)sendTxToSlave(CMD_UPDATE, "Kalibracja", true);
}

void sendMotorTest()
{
  (void)sendTxToSlave(CMD_MOTORTEST, "Motor run", false);
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


void handleCalibrate()
{
  const String offsetStr = server.arg("offset");
  float offsetValue = 0.0f;

  if (!parseFloatStrict(offsetStr, offsetValue))
  {
    server.send(400, "application/json", "{\"error\":\"Niepoprawny parametr offset\"}");
    return;
  }

  if (offsetValue < -14.999f || offsetValue > 14.999f)
  {
    server.send(400, "application/json", "{\"error\":\"Offset poza zakresem (-14.999..14.999)\"}");
    return;
  }

  // Zapisz offset (lokalnie na Master)
  systemStatus.localCalibrationOffset = offsetValue;
  DEBUG_PLOT("calibrationOffset:%.3f", (double)systemStatus.localCalibrationOffset);
  systemStatus.localCalibrationOffset = offsetValue;
  DEBUG_PLOT("calibrationOffset:%.3f", (double)systemStatus.localCalibrationOffset);

  // Wykonaj pomiar
  requestMeasurement();

  // Poczekaj na wynik
  delay(1000);

  // Wyślij błąd przez Serial
  if (measurementReady)
  {
    calibrationError = lastDeviationValue;
    DEBUG_PLOT("calibrationError:%.3f", (double)calibrationError);
  }

  String response = "{\"offset\":" + String((double)systemStatus.localCalibrationOffset, 3) + ",\"error\":" +
                    String((double)calibrationError, 3) + "}";
  server.send(200, "application/json", response);
}

void handleStartSession()
{
  String sessionName = server.arg("sessionName");
  sessionName.replace("%20", " "); // Zamień spacje z URL encoding

  if (sessionName.length() == 0)
  {
    server.send(400, "application/json", "{\"error\":\"Nazwa sesji nie może być pusta\"}");
    return;
  }

  currentSessionName = sessionName;
  sessionActive = true;

  String response = "{\"sessionName\":\"" + sessionName + "\",\"active\":true}";
  server.send(200, "application/json", response);
}

void handleMeasureSession()
{
  if (!sessionActive)
  {
    server.send(400, "application/json", "{\"error\":\"Sesja nieaktywna\"}");
    return;
  }

  requestMeasurement();

  // Poczekaj na wynik
  delay(1000);

  if (measurementReady)
  {
    // Wyślij przez Serial
    DEBUG_PLOT("measurementReady:%s %.3f", currentSessionName.c_str(), (double)lastDeviationValue);
  }

  const MessageSlave &m = systemStatus.msgSlave;

  String response = "{";
  response += "\"sessionName\":\"" + currentSessionName + "\",";
  response += "\"measurement\":\"" + lastMeasurement + "\",";
  response += "\"valid\":" + String(measurementReady ? "true" : "false") + ",";
  response += "\"timestamp\":" + String((unsigned)m.timestamp) + ",";
  response += "\"batteryVoltage\":" + String(m.batteryVoltage, 3) + ",";
  response += "\"angleX\":" + String((unsigned)m.angleX);
  response += "}";

  server.send(200, "application/json", response);
}

void printSerialHelp()
{
  DEBUG_I("\n=== DOSTĘPNE KOMENDY SERIAL (UART) ===\n"
          "m            - Wyślij do slave: CMD_MEASURE (M)\n"
          "u            - Wyślij do slave: CMD_UPDATE (U)\n"
          "o <ms>       - Ustaw msgMaster.timeout (timeout)\n"
          "q <0-255>    - Ustaw msgMaster.motorTorque\n"
          "s <0-255>    - Ustaw msgMaster.motorSpeed\n"
          "r <0-3>      - Ustaw msgMaster.motorState i wyślij CMD_MOTORTEST (T)\n"
          "t            - Wyślij CMD_MOTORTEST (T) z bieżących pól msgMaster\n"
          "o <ms>       - Ustaw msgMaster.timeout (timeout)\n"
          "q <0-255>    - Ustaw msgMaster.motorTorque\n"
          "s <0-255>    - Ustaw msgMaster.motorSpeed\n"
          "r <0-3>      - Ustaw msgMaster.motorState i wyślij CMD_MOTORTEST (T)\n"
          "t            - Wyślij CMD_MOTORTEST (T) z bieżących pól msgMaster\n"
          "c <±14.999>  - Ustaw offset kalibracji w mm (lokalnie na Master)\n"
          "h/?          - Wyświetl tę pomoc\n"
          "=====================================\n");
}

static bool parseIntStrict(const String &s, long &out)
{
  const char *p = s.c_str();
  while (*p == ' ' || *p == '\t')
  {
    ++p;
  }

  if (*p == '\0')
  {
    return false;
  }

  char *end = nullptr;
  out = strtol(p, &end, 10);

  if (end == p)
  {
    return false;
  }

  while (*end == ' ' || *end == '\t')
  {
    ++end;
  }

  return (*end == '\0');
}

static bool parseFloatStrict(const String &s, float &out)
{
  const char *p = s.c_str();
  while (*p == ' ' || *p == '\t')
  {
    ++p;
  }

  if (*p == '\0')
  {
    return false;
  }

  char *end = nullptr;
  out = strtof(p, &end);

  if (end == p)
  {
    return false;
  }

  while (*end == ' ' || *end == '\t')
  {
    ++end;
  }

  return (*end == '\0');
}

bool serialCommandParser(void *arg)
{
  (void)arg;

  // Parser liniowy: czytamy do '\n' bez blokowania.
  static String lineBuf;

  while (Serial.available() > 0)
  {
    const char ch = (char)Serial.read();

    if (ch == '\r')
    {
      continue;
    }

    if (ch != '\n')
    {
      // Ograniczenie długości linii, żeby nie rozjechać RAM przy śmieciach na Serial.
      if (lineBuf.length() < 64)
      {
        lineBuf += ch;
      }
      continue;
    }

    // Mamy pełną linię
    String line = lineBuf;
    lineBuf = "";

    line.trim();
    if (line.length() == 0)
    {
      continue;
    }

    const char cmd = line.charAt(0);
    String rest = line.substring(1);
    rest.trim();

    long val = 0;
    float fval = 0.0f;

    switch (cmd)
    {
    case 'm':
      requestMeasurement();
      break;

    case 'o':
      if (!parseIntStrict(rest, val))
      {
        DEBUG_W("Serial: brak/niepoprawny parametr dla 'o' (użyj: o <ms>\\n)");
        printSerialHelp();
        break;
      }

      if (val < 0 || val > 600000)
      {
        DEBUG_W("Serial: timeout poza zakresem: %ld (0..600000 ms)", val);
        break;
      }

      systemStatus.msgMaster.timeout = (uint32_t)val;
      DEBUG_I("tx.timeout:%u", (unsigned)systemStatus.msgMaster.timeout);
      systemStatus.msgMaster.timeout = (uint32_t)val;
      DEBUG_I("tx.timeout:%u", (unsigned)systemStatus.msgMaster.timeout);
      break;

    case 'u':
      requestUpdate();
      break;

    case 'c':
      if (!parseFloatStrict(rest, fval))
      {
        DEBUG_W("Serial: brak/niepoprawny parametr dla 'c' (użyj: c <offset_mm>\\n)");
        printSerialHelp();
        break;
      }

      if (fval < -14.999f || fval > 14.999f)
      {
        DEBUG_W("Serial: calibrationOffset poza zakresem: %.3f (-14.999..14.999)", (double)fval);
        break;
      }

      systemStatus.localCalibrationOffset = fval;
      DEBUG_I("localCalibrationOffset:%.3f", (double)systemStatus.localCalibrationOffset);
      systemStatus.localCalibrationOffset = fval;
      DEBUG_I("localCalibrationOffset:%.3f", (double)systemStatus.localCalibrationOffset);
      requestCalibration();
      break;

    case 'q':
      if (!parseIntStrict(rest, val))
      {
        DEBUG_W("Serial: brak/niepoprawny parametr dla 'q' (użyj: q <0-255>\\n)");
        printSerialHelp();
        break;
      }

      if (val < 0 || val > 255)
      {
        DEBUG_W("Serial: motorTorque poza zakresem: %ld (0..255)", val);
        break;
      }

      systemStatus.msgMaster.motorTorque = (uint8_t)val;
      DEBUG_I("tx.motorTorque:%u", (unsigned)systemStatus.msgMaster.motorTorque);
      systemStatus.msgMaster.motorTorque = (uint8_t)val;
      DEBUG_I("tx.motorTorque:%u", (unsigned)systemStatus.msgMaster.motorTorque);
      break;

    case 's':
      if (!parseIntStrict(rest, val))
      {
        DEBUG_W("Serial: brak/niepoprawny parametr dla 's' (użyj: s <0-255>\\n)");
        printSerialHelp();
        break;
      }

      if (val < 0 || val > 255)
      {
        DEBUG_W("Serial: motorSpeed poza zakresem: %ld (0..255)", val);
        break;
      }

      systemStatus.msgMaster.motorSpeed = (uint8_t)val;
      DEBUG_I("tx.motorSpeed:%u", (unsigned)systemStatus.msgMaster.motorSpeed);
      systemStatus.msgMaster.motorSpeed = (uint8_t)val;
      DEBUG_I("tx.motorSpeed:%u", (unsigned)systemStatus.msgMaster.motorSpeed);
      break;

    case 'r':
      if (!parseIntStrict(rest, val))
      {
        DEBUG_W("Serial: brak/niepoprawny parametr dla 'r' (użyj: r <0-3>\\n)");
        printSerialHelp();
        break;
      }

      if (val < 0 || val > 3)
      {
        DEBUG_W("Serial: motorState poza zakresem: %ld (0..3)", val);
        break;
      }

      systemStatus.msgMaster.motorState = (MotorState)val;
      DEBUG_I("tx.motorState:%u", (unsigned)systemStatus.msgMaster.motorState);
      systemStatus.msgMaster.motorState = (MotorState)val;
      DEBUG_I("tx.motorState:%u", (unsigned)systemStatus.msgMaster.motorState);
      sendMotorTest();
      break;

    case 't':
      sendMotorTest();
      break;

    case 'h':
    case '?':
      printSerialHelp();
      break;

    default:
      DEBUG_W("Serial: nieznana komenda: '%c' (linia: %s)", cmd, line.c_str());
      printSerialHelp();
      break;
    }
  }

  return true;
}

void setup()
{
  DEBUG_BEGIN();
  DEBUG_I("=== ESP32 MASTER - Suwmiarka + ESP-NOW ===");

  // Initialize system status
  memset(&systemStatus, 0, sizeof(systemStatus));
  initDefaultTxMessage();

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
  server.on("/calibrate", HTTP_POST, handleCalibrate);
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

  timerWorker.every(200, serialCommandParser);
}

void loop()
{
  server.handleClient();
  timerWorker.tick();
}
