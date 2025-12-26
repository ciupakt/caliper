#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
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

void handleRoot()
{
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 System Pomiarowy</title>";
  html += "<style>";
  html += "body { font-family: Arial; max-width: 600px; margin: 50px auto; padding: 20px; background: #f0f0f0; }";
  html += ".container { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; text-align: center; }";
  html += ".view { display: block; }";
  html += ".hidden { display: none; }";
  html += ".measurement { font-size: 48px; text-align: center; color: #007bff; margin: 30px 0; padding: 20px; background: #e7f3ff; border-radius: 5px; }";
  html += "button { width: 100%; padding: 15px; font-size: 18px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; margin: 10px 0; }";
  html += "button:hover { background: #0056b3; }";
  html += "button.motor { width: 48%; float: left; margin: 1%; font-size: 14px; padding: 10px; }";
  html += "button.motor.forward { background: #28a745; }";
  html += "button.motor.forward:hover { background: #218838; }";
  html += "button.motor.reverse { background: #ffc107; color: black; }";
  html += "button.motor.reverse:hover { background: #e0a800; }";
  html += "button.motor.stop { background: #dc3545; }";
  html += "button.motor.stop:hover { background: #c82333; }";
  html += ".status { text-align: center; color: #666; margin-top: 20px; font-size: 14px; }";
  html += "input { width: 100%; padding: 15px; font-size: 18px; border: 1px solid #ddd; border-radius: 5px; margin: 10px 0; box-sizing: border-box; outline: none; }";
  html += "input:focus { border-color: #007bff; box-shadow: 0 0 5px rgba(0,123,255,0.5); }";
  html += ".result { text-align: center; font-size: 16px; color: #333; margin: 15px 0; padding: 10px; background: #f8f9fa; border-radius: 5px; }";
  html += "</style></head><body>";
  html += "<div class='container'>";

  // Widok Menu
  html += "<div id='menu-view' class='view'>";
  html += "<h1>System Pomiarowy ESP32</h1>";
  html += "<button onclick='showView(\"calibration\")'>Kalibracja</button>";
  html += "<button onclick='showView(\"session-name\")'>Nowa sesja pomiarowa</button>";
  html += "</div>";

  // Widok Kalibracja
  html += "<div id='calibration-view' class='view hidden'>";
  html += "<h1>Kalibracja</h1>";
  html += "<input type='number' id='offset-input' min='74' max='165' placeholder='Offset (74-165)'>";
  html += "<button onclick='calibrate()'>Kalibruj</button>";
  html += "<button onclick='showView(\"menu\")'>Menu</button>";
  html += "<div id='calibration-result' class='result'></div>";
  html += "</div>";

  // Widok Nazwa Sesji
  html += "<div id='session-name-view' class='view hidden'>";
  html += "<h1>Nowa Sesja Pomiarowa</h1>";
  html += "<input type='text' id='session-name-input' placeholder='Nazwa sesji' autocomplete='off' autofocus>";
  html += "<button onclick='startSession()'>Start</button>";
  html += "<button onclick='showView(\"menu\")'>Menu</button>";
  html += "</div>";

  // Widok Pomiarów
  html += "<div id='measurement-view' class='view hidden'>";
  html += "<h1>Sesja: <span id='session-name-display'></span></h1>";
  html += "<div class='measurement' id='measurement-value'>" + lastMeasurement + "</div>";
  html += "<div style='text-align: center; font-size: 18px; color: #666; margin: 10px 0;'>Napięcie baterii: <span id='battery'>" + lastBatteryVoltage + "</span></div>";
  html += "<button onclick='measureSession()'>Wykonaj pomiar</button>";
  html += "<button onclick='refreshSession()'>Odśwież wynik</button>";
  html += "<button class='motor forward' onclick='motorForward()'>Forward</button>";
  html += "<button class='motor reverse' onclick='motorReverse()'>Reverse</button>";
  html += "<button class='motor stop' onclick='motorStop()'>Stop</button>";
  html += "<button onclick='showView(\"menu\")'>Menu</button>";
  html += "<div class='status' id='status'></div>";
  html += "</div>";

  html += "</div>";
  html += "<script>";

  // Funkcje zarządzania widokami
  html += "function showView(viewId) {";
  html += "  document.querySelectorAll('.view').forEach(view => {";
  html += "    view.classList.add('hidden');";
  html += "  });";
  html += "  document.getElementById(viewId + '-view').classList.remove('hidden');";
  html += "}";

  // Funkcje kalibracji
  html += "function calibrate() {";
  html += "  const offset = document.getElementById('offset-input').value;";
  html += "  if (offset < 74 || offset > 165) {";
  html += "    alert('Offset musi być w zakresie 74-165');";
  html += "    return;";
  html += "  }";
  html += "  document.getElementById('status').textContent = 'Wykonywanie kalibracji...';";
  html += "  fetch('/calibrate', {";
  html += "    method: 'POST',";
  html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
  html += "    body: 'offset=' + offset";
  html += "  })";
  html += "    .then(response => {";
  html += "      if (!response.ok) {";
  html += "        return response.json().then(err => { throw new Error(err.error || 'Błąd serwera'); });";
  html += "      }";
  html += "      return response.json();";
  html += "    })";
  html += "    .then(data => {";
  html += "      if (data.error && !data.offset) {";
  html += "        document.getElementById('status').textContent = 'Błąd: ' + data.error;";
  html += "      } else {";
  html += "        document.getElementById('calibration-result').innerHTML = 'Offset: ' + data.offset + '<br>Błąd: ' + data.error + ' mm';";
  html += "        document.getElementById('status').textContent = 'Kalibracja zakończona';";
  html += "      }";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Błąd: ' + error.message;";
  html += "    });";
  html += "}";

  // Funkcje sesji pomiarowej
  html += "function startSession() {";
  html += "  const sessionName = document.getElementById('session-name-input').value;";
  html += "  if (!sessionName) {";
  html += "    alert('Podaj nazwę sesji');";
  html += "    return;";
  html += "  }";
  html += "  fetch('/start_session', {";
  html += "    method: 'POST',";
  html += "    headers: {'Content-Type': 'application/x-www-form-urlencoded'},";
  html += "    body: 'sessionName=' + encodeURIComponent(sessionName)";
  html += "  })";
  html += "    .then(response => {";
  html += "      if (!response.ok) {";
  html += "        return response.json().then(err => { throw new Error(err.error || 'Błąd serwera'); });";
  html += "      }";
  html += "      return response.json();";
  html += "    })";
  html += "    .then(data => {";
  html += "      if (data.error) {";
  html += "        document.getElementById('status').textContent = 'Błąd: ' + data.error;";
  html += "      } else {";
  html += "        document.getElementById('session-name-display').textContent = sessionName;";
  html += "        showView('measurement');";
  html += "      }";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Błąd: ' + error.message;";
  html += "    });";
  html += "}";

  html += "function measureSession() {";
  html += "  document.getElementById('status').textContent = 'Wysyłanie zadania...';";
  html += "  fetch('/measure_session', {";
  html += "    method: 'POST'";
  html += "  })";
  html += "    .then(response => {";
  html += "      if (!response.ok) {";
  html += "        return response.json().then(err => { throw new Error(err.error || 'Błąd serwera'); });";
  html += "      }";
  html += "      return response.json();";
  html += "    })";
  html += "    .then(data => {";
  html += "      if (data.error) {";
  html += "        document.getElementById('status').textContent = 'Błąd: ' + data.error;";
  html += "      } else {";
  html += "        document.getElementById('measurement-value').textContent = data.measurement;";
  html += "        document.getElementById('status').textContent = 'Zadanie wysłane! Odczekaj chwile i odśwież.';";
  html += "        setTimeout(refreshSession, 1500);";
  html += "      }";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Błąd: ' + error.message;";
  html += "    });";
  html += "}";

  html += "function refreshSession() {";
  html += "  document.getElementById('status').textContent = 'Pobieranie danych...';";
  html += "  fetch('/read')";
  html += "    .then(response => {";
  html += "      if (!response.ok) {";
  html += "        throw new Error('Błąd serwera: ' + response.status);";
  html += "      }";
  html += "      return response.text();";
  html += "    })";
  html += "    .then(data => {";
  html += "      document.getElementById('measurement-value').textContent = data;";
  html += "      document.getElementById('status').textContent = 'Zaktualizowano: ' + new Date().toLocaleTimeString();";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Błąd: ' + error.message;";
  html += "    });";
  html += "}";

  // Funkcje sterowania silnikiem
  html += "function motorForward() {";
  html += "  document.getElementById('status').textContent = 'Wysyłanie komendy Forward...';";
  html += "  fetch('/forward')";
  html += "    .then(response => {";
  html += "      if (!response.ok) {";
  html += "        throw new Error('Błąd serwera: ' + response.status);";
  html += "      }";
  html += "      return response.text();";
  html += "    })";
  html += "    .then(data => {";
  html += "      document.getElementById('status').textContent = 'Forward: ' + data;";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Błąd: ' + error.message;";
  html += "    });";
  html += "}";

  html += "function motorReverse() {";
  html += "  document.getElementById('status').textContent = 'Wysyłanie komendy Reverse...';";
  html += "  fetch('/reverse')";
  html += "    .then(response => {";
  html += "      if (!response.ok) {";
  html += "        throw new Error('Błąd serwera: ' + response.status);";
  html += "      }";
  html += "      return response.text();";
  html += "    })";
  html += "    .then(data => {";
  html += "      document.getElementById('status').textContent = 'Reverse: ' + data;";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Błąd: ' + error.message;";
  html += "    });";
  html += "}";

  html += "function motorStop() {";
  html += "  document.getElementById('status').textContent = 'Wysyłanie komendy Stop...';";
  html += "  fetch('/stop')";
  html += "    .then(response => {";
  html += "      if (!response.ok) {";
  html += "        throw new Error('Błąd serwera: ' + response.status);";
  html += "      }";
  html += "      return response.text();";
  html += "    })";
  html += "    .then(data => {";
  html += "      document.getElementById('status').textContent = 'Stop: ' + data;";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Błąd: ' + error.message;";
  html += "    });";
  html += "}";

  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
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

  // Setup web server routes
  server.on("/", handleRoot);
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
