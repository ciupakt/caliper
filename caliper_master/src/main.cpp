#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"
#include <shared_common.h>
#include "communication.h"

// Slave device MAC address (defined in config.h)
uint8_t slaveAddress[] = SLAVE_MAC_ADDR;

// Global objects
WebServer server(WEB_SERVER_PORT);
CommunicationManager commManager;
SystemStatus systemStatus;
Message receivedData;

// Global variables
String lastMeasurement = "Brak pomiaru";
String lastBatteryVoltage = "Brak danych";
bool measurementReady = false;

// Session management variables
String currentSessionName = "";
bool sessionActive = false;
int calibrationOffset = 0;
float calibrationError = 0.0;

String macToString(const uint8_t *mac)
{
  char buf[18];
  sprintf(buf, "%02X,%02X,%02X,%02X,%02X,%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  if (len != sizeof(receivedData))
  {
    Serial.println("BLAD: Nieprawidlowa dlugosc pakietu ESP-NOW");
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
      Serial.println("BLAD: Wartosc pomiaru poza zakresem!");
      lastMeasurement = "BLAD: Wartosc poza zakresem";
      measurementReady = true;
      systemStatus.measurementValid = false;
      return;
    }

    if (receivedData.valid)
    {
      lastMeasurement = String(receivedData.measurement, 3) + " mm";
      Serial.print("VAL_1:");
      Serial.println(receivedData.measurement, 3);
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

    Serial.println("\n=== OTRZYMANO WYNIK POMIARU ===");
    Serial.print("Wartosc: ");
    Serial.println(lastMeasurement);
    Serial.print("Timestamp: ");
    Serial.println(receivedData.timestamp);
    Serial.print(">Napiecie baterii:");
    Serial.print(receivedData.batteryVoltage);
    Serial.println("mV");
    Serial.println("================================\n");
  }
  else if (receivedData.command == CMD_UPDATE)
  {
    // Status update
    Serial.println("\n=== AKTUALIZACJA STATUSU ===");
    Serial.print(">Napiecie baterii:");
    Serial.print(receivedData.batteryVoltage);
    Serial.println(" mV");
    lastBatteryVoltage = String(receivedData.batteryVoltage) + " mV";
    systemStatus.batteryVoltage = receivedData.batteryVoltage;
    Serial.println("==============================\n");
  }
}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  if (status != ESP_NOW_SEND_SUCCESS)
  {
    Serial.println("ERR: Wyslanie nie powiodlo sie!");
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
    Serial.print("Wyslano komendę: ");
    Serial.println(commandName);
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
    Serial.print("BLAD wysylania komendy ");
    Serial.print(commandName);
    Serial.print(": ");
    Serial.println(result);
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
  Serial.print("CAL_OFFSET:");
  Serial.println(offsetValue);

  // Wykonaj pomiar
  requestMeasurement();

  // Poczekaj na wynik
  delay(1000);

  // Wyślij błąd przez Serial
  if (systemStatus.measurementValid)
  {
    calibrationError = systemStatus.lastMeasurement;
    Serial.print("CAL_ERROR:");
    Serial.println(calibrationError, 3);
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
    Serial.print("MEAS_SESSION:");
    Serial.print(currentSessionName);
    Serial.print(" ");
    Serial.println(systemStatus.lastMeasurement, 3);
  }

  String response = "{\"sessionName\":\"" + currentSessionName +
                    "\",\"measurement\":\"" + lastMeasurement + "\"}";
  server.send(200, "application/json", response);
}

void printSerialHelp()
{
  Serial.println("\n=== DOSTĘPNE KOMENDY SERIAL ===");
  Serial.println("M/m - Wykonaj pomiar");
  Serial.println("F/f - Silnik do przodu (Forward)");
  Serial.println("R/r - Silnik do tyłu (Reverse)");
  Serial.println("S/s - Zatrzymaj silnik (Stop)");
  Serial.println("H/h/? - Wyświetl tę pomoc");
  Serial.println("===============================\n");
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Initialize system status
  memset(&systemStatus, 0, sizeof(systemStatus));
  systemStatus.motorDirection = MOTOR_STOP;

  // Initialize LittleFS
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS Mount Failed!");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  // Setup WiFi
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("\n=== Access Point uruchomiony ===");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("================================\n");
  Serial.print("MAC Address Master: ");
  Serial.println(WiFi.macAddress());
  Serial.println();

  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);

  // Initialize communication manager
  if (commManager.initialize(slaveAddress) != ERR_NONE)
  {
    Serial.println("ESP-NOW init failed!");
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
  Serial.println("Serwer HTTP uruchomiony na porcie " + String(WEB_SERVER_PORT));
  Serial.println("Polacz sie z WiFi: " + String(WIFI_SSID));
  Serial.println("Otworz: http://" + WiFi.softAPIP().toString());
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
      Serial.println("Nieznana komenda. Wpisz 'H' lub '?' aby zobaczyć dostępne komendy.");
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
