#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"
#include <shared_common.h>
#include <MacroDebugger.h>
#include "communication.h"

// Slave device MAC address (defined in config.h)
uint8_t slaveAddress[] = SLAVE_MAC_ADDR;

// Global objects
WebServer server(WEB_SERVER_PORT);
CommunicationManager commManager;
SystemStatus systemStatus;
Message msg;

// Global variables
String lastMeasurement = "Brak pomiaru";
String lastBatteryVoltage = "Brak danych";
bool measurementReady = false;

// Session management variables
String currentSessionName = "";
bool sessionActive = false;
int calibrationOffset = 0;
float calibrationError = 0.0;

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  if (len != sizeof(receivedData))
  {
    DEBUG_E("BLAD: Nieprawidlowa dlugosc pakietu ESP-NOW");
    return;
  }

  memcpy(&receivedData, incomingData, sizeof(receivedData));

  // Update system status
  systemStatus.lastUpdate = millis();
  systemStatus.communicationActive = true;

  // Handle different command types
  if (receivedData.command == CMD_MEASURE)
  {
    // Validate measurement range
    if (receivedData.valid && (receivedData.measurement < MEASUREMENT_MIN_VALUE || receivedData.measurement > MEASUREMENT_MAX_VALUE))
    {
      DEBUG_E("BLAD: Wartosc pomiaru poza zakresem!");
      lastMeasurement = "BLAD: Wartosc poza zakresem";
      measurementReady = true;
      systemStatus.measurementValid = false;
      return;
    }

    if (receivedData.valid)
    {
      lastMeasurement = String(receivedData.measurement, 3) + " mm";
      DEBUG_PLOT("VAL_1:%.3f", (double)receivedData.measurement);
      systemStatus.lastMeasurement = receivedData.measurement;
      systemStatus.measurementValid = true;
    }
    else
    {
      lastMeasurement = "BLAD POMIARU";
      systemStatus.measurementValid = false;
    }
    lastBatteryVoltage = String(receivedData.batteryVoltage) + " mV";
    systemStatus.batteryVoltage = receivedData.batteryVoltage;
    measurementReady = true;

    DEBUG_I("\n=== OTRZYMANO WYNIK POMIARU ===");
    DEBUG_I("Wartosc: %s", lastMeasurement.c_str());
    DEBUG_I("Timestamp: %lu", (unsigned long)receivedData.timestamp);
    DEBUG_PLOT("Napiecie baterii:%u mV", (unsigned int)receivedData.batteryVoltage);
    DEBUG_PLOT("Angle X:%.2f", (double)receivedData.angleX);
    DEBUG_I("================================\n");
  }
  else if (receivedData.command == CMD_UPDATE)
  {
    // Status update
    DEBUG_I("\n=== AKTUALIZACJA STATUSU ===");
    DEBUG_PLOT("Napiecie baterii:%u mV", (unsigned int)receivedData.batteryVoltage);
    DEBUG_PLOT("Angle X:%.2f", (double)receivedData.angleX);
    lastBatteryVoltage = String(receivedData.batteryVoltage) + " mV";
    systemStatus.batteryVoltage = receivedData.batteryVoltage;
    DEBUG_I("==============================\n");
  }
}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  if (status != ESP_NOW_SEND_SUCCESS)
  {
    DEBUG_E("ERR: Wyslanie nie powiodlo sie!");
    systemStatus.communicationActive = false;
  }
  else
  {
    systemStatus.communicationActive = true;
  }
}

// Unified command sending function
ErrorCode sendCommand(CommandType command, const char *commandName)
{
  measurementReady = false;
  lastMeasurement = "Oczekiwanie na odpowiedź...";

  ErrorCode result = commManager.sendCommand(command);

  if (result == ERR_NONE)
  {
    DEBUG_I("Wyslano komendę: %s", commandName);
    lastMeasurement = String("Komenda: ") + commandName;

    // Update motor status if it's a motor command
    if (command == CMD_FORWARD || command == CMD_REVERSE || command == CMD_STOP)
    {
      systemStatus.motorRunning = (command != CMD_STOP);
      systemStatus.motorDirection = (command == CMD_FORWARD) ? MOTOR_FORWARD : (command == CMD_REVERSE) ? MOTOR_REVERSE
                                                                                                         : MOTOR_STOP;
    }
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
  sendCommand(CMD_MEASURE, "Pomiar");
}

void sendMotorForward()
{
  sendCommand(CMD_FORWARD, "Silnik do przodu");
}

void sendMotorReverse()
{
  sendCommand(CMD_REVERSE, "Silnik do tyłu");
}

void sendMotorStop()
{
  sendCommand(CMD_STOP, "Zatrzymaj silnik");
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

void handleAPI()
{
  String json = "{";
  json += "\"measurement\":\"" + lastMeasurement + "\",";
  json += "\"timestamp\":" + String(receivedData.timestamp) + ",";
  json += "\"valid\":" + String(receivedData.valid ? "true" : "false") + ",";
  json += "\"batteryVoltage\":" + String(receivedData.batteryVoltage) + ",";
  json += "\"angleX\":" + String(receivedData.angleX, 2) + ",";
  json += "\"command\":\"" + String(receivedData.command) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleMotorForward()
{
  sendMotorForward();
  server.send(200, "text/plain", "Silnik: Forward");
}

void handleMotorReverse()
{
  sendMotorReverse();
  server.send(200, "text/plain", "Silnik: Reverse");
}

void handleMotorStop()
{
  sendMotorStop();
  server.send(200, "text/plain", "Silnik: Stop");
}

void handleCalibrate()
{
  String offset = server.arg("offset");
  int offsetValue = offset.toInt();

  if (offsetValue < 74 || offsetValue > 165)
  {
    server.send(400, "application/json", "{\"error\":\"Offset poza zakresem\"}");
    return;
  }

  // Zapisz offset i wyślij przez Serial
  calibrationOffset = offsetValue;
  DEBUG_PLOT("CAL_OFFSET:%d", calibrationOffset);

  // Wykonaj pomiar
  requestMeasurement();

  // Poczekaj na wynik
  delay(1000);

  // Wyślij błąd przez Serial
  if (systemStatus.measurementValid)
  {
    calibrationError = systemStatus.lastMeasurement;
    DEBUG_PLOT("CAL_ERROR:%.3f", (double)calibrationError);
  }

  String response = "{\"offset\":" + offset + ",\"error\":" +
                    String(calibrationError, 3) + "}";
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

  if (systemStatus.measurementValid)
  {
    // Wyślij przez Serial
    DEBUG_PLOT("MEAS_SESSION:%s %.3f", currentSessionName.c_str(), (double)systemStatus.lastMeasurement);
  }

  String response = "{\"sessionName\":\"" + currentSessionName +
                    "\",\"measurement\":\"" + lastMeasurement + "\"}";
  server.send(200, "application/json", response);
}

void printSerialHelp()
{
  DEBUG_I("\n=== DOSTĘPNE KOMENDY SERIAL ===\n"
          "M/m - Wykonaj pomiar\n"
          "F/f - Silnik do przodu (Forward)\n"
          "R/r - Silnik do tyłu (Reverse)\n"
          "S/s - Zatrzymaj silnik (Stop)\n"
          "H/h/? - Wyświetl tę pomoc\n"
          "===============================\n");
}

void setup()
{
  DEBUG_BEGIN();
  DEBUG_I("=== ESP32 MASTER - Suwmiarka + ESP-NOW ===");

  // Initialize system status
  memset(&systemStatus, 0, sizeof(systemStatus));
  systemStatus.motorDirection = MOTOR_STOP;

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
  server.on("/api", handleAPI);
  server.on("/forward", handleMotorForward);
  server.on("/reverse", handleMotorReverse);
  server.on("/stop", handleMotorStop);
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
}

void loop()
{
  server.handleClient();

  if (Serial.available())
  {
    char input = Serial.read();
    switch (input)
    {
    case 'M':
    case 'm':
      requestMeasurement();
      break;
    case 'F':
    case 'f':
      sendMotorForward();
      break;
    case 'R':
    case 'r':
      sendMotorReverse();
      break;
    case 'S':
    case 's':
      sendMotorStop();
      break;
    case 'H':
    case 'h':
    case '?':
      printSerialHelp();
      break;
    default:
      DEBUG_W("Nieznana komenda. Wpisz 'H' lub '?' aby zobaczyć dostępne komendy.");
      break;
    }
  }

  // Zamiast delay(10) - lepsza responsywność
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 10)
  {
    lastCheck = millis();
    // Możliwość dodania innych zadań
  }
}
