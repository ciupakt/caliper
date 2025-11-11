#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>

#define ESPNOW_WIFI_CHANNEL 1

uint8_t slaveAddress[] = {0xA0, 0xB7, 0x65, 0x21, 0x77, 0x5C};

// Dane Access Point
const char* ssid = "ESP32_Pomiar";
const char* password = "12345678";

typedef struct struct_message {
  float measurement;
  bool valid;
  uint32_t timestamp;
  char command;          // Command type: 'M' = measurement, 'F' = forward, 'R' = reverse, 'S' = stop, 'D' = demo
  float motorCurrent;    // Motor current reading
  bool motorFault;       // Motor fault status
} struct_message;

struct_message receivedData;
esp_now_peer_info_t peerInfo;

WebServer server(80);

String lastMeasurement = "Brak pomiaru";
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

  // Obsługa różnych typów komend
  if (receivedData.command == 'M') {
    // Walidacja zakresu pomiaru
    if (receivedData.valid && (receivedData.measurement < -1000.0 || receivedData.measurement > 1000.0)) {
      Serial.println("BLAD: Wartosc pomiaru poza zakresem!");
      lastMeasurement = "BLAD: Wartosc poza zakresem";
      measurementReady = true;
      return;
    }

    if (receivedData.valid) {
      lastMeasurement = String(receivedData.measurement, 3) + " mm";
      Serial.print("VAL_1:");
      Serial.println(receivedData.measurement, 3);
    } else {
      lastMeasurement = "BLAD POMIARU";
    }
    measurementReady = true;

    Serial.println("\n=== OTRZYMANO WYNIK POMIARU ===");
    Serial.print("Wartosc: ");
    Serial.println(lastMeasurement);
    Serial.print("Timestamp: ");
    Serial.println(receivedData.timestamp);
    Serial.print("Prad silnika: ");
    Serial.print(receivedData.motorCurrent, 3);
    Serial.print("A, Blad silnika: ");
    Serial.println(receivedData.motorFault ? "TAK" : "NIE");
    Serial.println("================================\n");
  } else if (receivedData.command == 'U') {
    // Aktualizacja statusu silnika
    Serial.println("\n=== AKTUALIZACJA STATUSU SILNIKA ===");
    Serial.print("Prad silnika: ");
    Serial.print(receivedData.motorCurrent, 3);
    Serial.print("A, Blad silnika: ");
    Serial.println(receivedData.motorFault ? "TAK" : "NIE");
    Serial.println("=====================================\n");
  }
}

// POPRAWKA: Zmieniona sygnatura callbacka
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("ERR: Wyslanie nie powiodlo sie!");
  }
}

void requestMeasurement() {
  uint8_t command = 'M';
  measurementReady = false;
  lastMeasurement = "Oczekiwanie na pomiar...";

  esp_err_t result = esp_now_send(slaveAddress, &command, sizeof(command));
  if (result == ESP_OK) {
    Serial.println("Wyslano zadanie pomiaru");
  } else {
    Serial.print("BLAD wysylania zadania: ");
    Serial.println(result);
    lastMeasurement = "BLAD: Nie mozna wyslac zadania";

    // Próba ponownego wysłania po krótkiej przerwie
    delay(100);
    result = esp_now_send(slaveAddress, &command, sizeof(command));
    if (result == ESP_OK) {
      Serial.println("Ponowne wyslanie udane");
      lastMeasurement = "Oczekiwanie na pomiar...";
    } else {
      Serial.println("Ponowne wyslanie nieudane");
    }
  }
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
  html += ".status { text-align: center; color: #666; margin-top: 20px; font-size: 14px; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>System Pomiarowy ESP32</h1>";
  html += "<div class='measurement' id='value'>" + lastMeasurement + "</div>";
  html += "<button onclick='measure()'>Wykonaj Pomiar</button>";
  html += "<button onclick='refresh()'>Odswiez Wynik</button>";
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
  json += "\"motorCurrent\":" + String(receivedData.motorCurrent, 3) + ",";
  json += "\"motorFault\":" + String(receivedData.motorFault ? "true" : "false") + ",";
  json += "\"command\":\"" + String(receivedData.command) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, password);
  
  Serial.println("\n=== Access Point uruchomiony ===");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("================================\n");
  Serial.print("MAC Address Master: ");
  Serial.println(WiFi.macAddress());
  Serial.println();

  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
  }

  server.on("/", handleRoot);
  server.on("/measure", handleMeasure);
  server.on("/read", handleRead);
  server.on("/api", handleAPI);
  
  server.begin();
  Serial.println("Serwer HTTP uruchomiony na porcie 80");
  Serial.println("Polacz sie z WiFi: " + String(ssid));
  Serial.println("Otworz: http://" + WiFi.softAPIP().toString());
}

void loop() {
  server.handleClient();

  if (Serial.available()) {
    char input = Serial.read();
    if (input == 'M' || input == 'm') {
      requestMeasurement();
    }
  }

  // Zamiast delay(10) - lepsza responsywność
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 10) {
    lastCheck = millis();
    // Możliwość dodania innych zadań
  }
}
