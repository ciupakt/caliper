#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "common.h"
#include "communication.h"

// Slave device MAC address
uint8_t slaveAddress[] = {0xA0, 0xB7, 0x65, 0x21, 0x77, 0x5C};

// Global objects
WebServer server(WEB_SERVER_PORT);
CommunicationManager commManager;
SystemStatus systemStatus;
Message receivedData;

// Global variables
String lastMeasurement = "Brak pomiaru";
String lastBatteryVoltage = "Brak danych";
bool measurementReady = false;

String macToString(const uint8_t *mac) {
  char buf[18];
  sprintf(buf, "%02X,%02X,%02X,%02X,%02X,%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len != sizeof(receivedData)) {
    Serial.println("BLAD: Nieprawidlowa dlugosc pakietu ESP-NOW");
    return;
  }

  memcpy(&receivedData, incomingData, sizeof(receivedData));

  // Update system status
  systemStatus.lastUpdate = millis();
  systemStatus.communicationActive = true;

  // Handle different command types
  if (receivedData.command == CMD_MEASURE) {
    // Validate measurement range
    if (receivedData.valid && (receivedData.measurement < MEASUREMENT_MIN_VALUE || receivedData.measurement > MEASUREMENT_MAX_VALUE)) {
      Serial.println("BLAD: Wartosc pomiaru poza zakresem!");
      lastMeasurement = "BLAD: Wartosc poza zakresem";
      measurementReady = true;
      systemStatus.measurementValid = false;
      return;
    }

    if (receivedData.valid) {
      lastMeasurement = String(receivedData.measurement, 3) + " mm";
      Serial.print("VAL_1:");
      Serial.println(receivedData.measurement, 3);
      systemStatus.lastMeasurement = receivedData.measurement;
      systemStatus.measurementValid = true;
    } else {
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
    Serial.print("Napiecie baterii: ");
    Serial.print(receivedData.batteryVoltage);
    Serial.println("mV");
    Serial.println("================================\n");
  } else if (receivedData.command == CMD_UPDATE) {
    // Status update
    Serial.println("\n=== AKTUALIZACJA STATUSU ===");
    Serial.print("Napiecie baterii: ");
    Serial.print(receivedData.batteryVoltage);
    Serial.println("mV");
    lastBatteryVoltage = String(receivedData.batteryVoltage) + " mV";
    systemStatus.batteryVoltage = receivedData.batteryVoltage;
    Serial.println("==============================\n");
  }
}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("ERR: Wyslanie nie powiodlo sie!");
    systemStatus.communicationActive = false;
  } else {
    systemStatus.communicationActive = true;
  }
}

// Unified command sending function
ErrorCode sendCommand(CommandType command, const char* commandName) {
  measurementReady = false;
  lastMeasurement = "Oczekiwanie na odpowiedź...";
  
  ErrorCode result = commManager.sendCommand(command);
  
  if (result == ERR_NONE) {
    Serial.print("Wyslano komendę: ");
    Serial.println(commandName);
    lastMeasurement = String("Komenda: ") + commandName;
    
    // Update motor status if it's a motor command
    if (command == CMD_FORWARD || command == CMD_REVERSE || command == CMD_STOP) {
      systemStatus.motorRunning = (command != CMD_STOP);
      systemStatus.motorDirection = (command == CMD_FORWARD) ? MOTOR_FORWARD :
                                   (command == CMD_REVERSE) ? MOTOR_REVERSE : MOTOR_STOP;
    }
  } else {
    Serial.print("BLAD wysylania komendy ");
    Serial.print(commandName);
    Serial.print(": ");
    Serial.println(result);
    lastMeasurement = "BLAD: Nie można wysłać komendy";
  }
  
  return result;
}

void requestMeasurement() {
  sendCommand(CMD_MEASURE, "Pomiar");
}

void sendMotorForward() {
  sendCommand(CMD_FORWARD, "Silnik do przodu");
}

void sendMotorReverse() {
  sendCommand(CMD_REVERSE, "Silnik do tyłu");
}

void sendMotorStop() {
  sendCommand(CMD_STOP, "Zatrzymaj silnik");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>ESP32 Pomiar</title>";
  html += "<style>";
  html += "body { font-family: Arial; max-width: 600px; margin: 50px auto; padding: 20px; background: #f0f0f0; }";
  html += ".container { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; text-align: center; }";
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
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>System Pomiarowy ESP32</h1>";
  html += "<div class='measurement' id='value'>" + lastMeasurement + "</div>";
  html += "<div style='text-align: center; font-size: 18px; color: #666; margin: 10px 0;'>Napięcie baterii: <span id='battery'>" + lastBatteryVoltage + "</span></div>";
  html += "<button onclick='measure()'>Wykonaj Pomiar</button>";
  html += "<button onclick='refresh()'>Odswiez Wynik</button>";
  html += "<button class='motor forward' onclick='motorForward()'>Forward</button>";
  html += "<button class='motor reverse' onclick='motorReverse()'>Reverse</button>";
  html += "<button class='motor stop' onclick='motorStop()'>Stop</button>";
  html += "<div class='status' id='status'></div>";
  html += "</div>";
  html += "<script>";
  html += "function measure() {";
  html += "  document.getElementById('status').textContent = 'Wysylanie zadania...';";
  html += "  fetch('/measure')";
  html += "    .then(response => response.text())";
  html += "    .then(data => {";
  html += "      document.getElementById('status').textContent = 'Zadanie wyslane! Odczekaj chwile i odswiez.';";
  html += "      setTimeout(refresh, 1500);";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Blad: ' + error;";
  html += "    });";
  html += "}";
  html += "function refresh() {";
  html += "  document.getElementById('status').textContent = 'Pobieranie danych...';";
  html += "  fetch('/read')";
  html += "    .then(response => response.text())";
  html += "    .then(data => {";
  html += "      document.getElementById('value').textContent = data;";
  html += "      document.getElementById('status').textContent = 'Zaktualizowano: ' + new Date().toLocaleTimeString();";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Blad: ' + error;";
  html += "    });";
  html += "}";
  html += "function motorForward() {";
  html += "  document.getElementById('status').textContent = 'Wysylanie komendy Forward...';";
  html += "  fetch('/forward')";
  html += "    .then(response => response.text())";
  html += "    .then(data => {";
  html += "      document.getElementById('status').textContent = 'Forward: ' + data;";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Blad: ' + error;";
  html += "    });";
  html += "}";
  html += "function motorReverse() {";
  html += "  document.getElementById('status').textContent = 'Wysylanie komendy Reverse...';";
  html += "  fetch('/reverse')";
  html += "    .then(response => response.text())";
  html += "    .then(data => {";
  html += "      document.getElementById('status').textContent = 'Reverse: ' + data;";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Blad: ' + error;";
  html += "    });";
  html += "}";
  html += "function motorStop() {";
  html += "  document.getElementById('status').textContent = 'Wysylanie komendy Stop...';";
  html += "  fetch('/stop')";
  html += "    .then(response => response.text())";
  html += "    .then(data => {";
  html += "      document.getElementById('status').textContent = 'Stop: ' + data;";
  html += "    })";
  html += "    .catch(error => {";
  html += "      document.getElementById('status').textContent = 'Blad: ' + error;";
  html += "    });";
  html += "}";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleMeasure() {
  requestMeasurement();
  server.send(200, "text/plain", "Pomiar wyzwolony");
}

void handleRead() {
  server.send(200, "text/plain", lastMeasurement);
}

void handleAPI() {
  String json = "{";
  json += "\"measurement\":\"" + lastMeasurement + "\",";
  json += "\"timestamp\":" + String(receivedData.timestamp) + ",";
  json += "\"valid\":" + String(receivedData.valid ? "true" : "false") + ",";
  json += "\"batteryVoltage\":" + String(receivedData.batteryVoltage) + ",";
  json += "\"command\":\"" + String(receivedData.command) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleMotorForward() {
  sendMotorForward();
  server.send(200, "text/plain", "Silnik: Forward");
}

void handleMotorReverse() {
  sendMotorReverse();
  server.send(200, "text/plain", "Silnik: Reverse");
}

void handleMotorStop() {
  sendMotorStop();
  server.send(200, "text/plain", "Silnik: Stop");
}


void printSerialHelp() {
  Serial.println("\n=== DOSTĘPNE KOMENDY SERIAL ===");
  Serial.println("M/m - Wykonaj pomiar");
  Serial.println("F/f - Silnik do przodu (Forward)");
  Serial.println("R/r - Silnik do tyłu (Reverse)");
  Serial.println("S/s - Zatrzymaj silnik (Stop)");
  Serial.println("H/h/? - Wyświetl tę pomoc");
  Serial.println("===============================\n");
}

void setup() {
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
  if (commManager.initialize(slaveAddress) != ERR_NONE) {
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
  
  server.begin();
  Serial.println("Serwer HTTP uruchomiony na porcie " + String(WEB_SERVER_PORT));
  Serial.println("Polacz sie z WiFi: " + String(WIFI_SSID));
  Serial.println("Otworz: http://" + WiFi.softAPIP().toString());
}

void loop() {
  server.handleClient();

  if (Serial.available()) {
    char input = Serial.read();
    switch (input) {
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
  if (millis() - lastCheck >= 10) {
    lastCheck = millis();
    // Możliwość dodania innych zadań
  }
}
