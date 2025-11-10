#include <esp_now.h>
#include <WiFi.h>

// #define CLOCK_PIN 23
// #define DATA_PIN 22
// #define TRIG_PIN 21

#define CLOCK_PIN 18
#define DATA_PIN 19
#define TRIG_PIN 5

#define ESPNOW_WIFI_CHANNEL 1

// Adres MAC mastera
uint8_t masterAddress[] = {0xA0, 0xB7, 0x65, 0x20, 0xC0, 0x8C};

volatile uint8_t bitBuffer[52];
volatile int bitCount = 0;
volatile bool dataReady = false;
volatile bool measurementRequested = false;

typedef struct struct_message {
  float measurement;
  bool valid;
  uint32_t timestamp;
} struct_message;

struct_message sensorData;
esp_now_peer_info_t peerInfo;

void IRAM_ATTR clockISR() {
  if (bitCount < 52) {
    uint8_t bit = digitalRead(DATA_PIN);
    bitBuffer[bitCount++] = bit;
    if (bitCount == 52) {
      dataReady = true;
    }
  }
}

void reverseBits() {
  for (int i = 0; i < 26; i++) {
    uint8_t temp = bitBuffer[i];
    bitBuffer[i] = bitBuffer[51 - i];
    bitBuffer[51 - i] = temp;
  }
}

float decodeCaliper() {
  uint8_t shifted[52];
  for (int i = 0; i < 52; i++) {
    if (i + 8 < 52) shifted[i] = bitBuffer[i + 8];
    else shifted[i] = 0;
  }
  uint8_t nibbles[13];
  for (int i = 0; i < 13; i++) {
    nibbles[i] = 0;
    for (int j = 0; j < 4; j++)
      nibbles[i] |= (shifted[i * 4 + (3 - j)] << j);
  }
  long value = 0;
  for (int i = 0; i < 5; i++)
    value += nibbles[i] * pow(10, i);
  bool negative = nibbles[6] & 0x08;
  bool inchMode = nibbles[6] & 0x04;
  float measurement = value / 1000.0;
  if (negative) measurement = -measurement;
  if (inchMode) measurement *= 25.4;
  return measurement;
}

float performMeasurement() {
  Serial.println("Wyzwalam pomiar TRIG...");
  digitalWrite(TRIG_PIN, LOW);

  bitCount = 0; dataReady = false;

  attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), clockISR, FALLING);

  unsigned long startTime = millis();
  unsigned long timeout = 200; // Zwiększony timeout
  while (!dataReady && (millis() - startTime < timeout)) {
    delayMicroseconds(100); // Krótsze opóźnienie dla lepszej responsywności
  }

  detachInterrupt(digitalPinToInterrupt(CLOCK_PIN));
  digitalWrite(TRIG_PIN, HIGH);

  if (dataReady) {
    reverseBits();
    float result = decodeCaliper();

    // Walidacja wyniku
    if (result >= -1000.0 && result <= 1000.0 && !isnan(result) && !isinf(result)) {
      Serial.print("Pomiar: ");
      Serial.print(result, 3);
      Serial.println(" mm");
      return result;
    } else {
      Serial.println("BLAD: Nieprawidlowa wartosc pomiaru!");
      return -999.0;
    }
  } else {
    Serial.println("BŁĄD: Timeout!");
    return -999.0;
  }
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len == 1 && incomingData[0] == 'M') {
    measurementRequested = true;
    Serial.println("Otrzymano zadanie pomiaru");
  } else {
    Serial.println("BLAD: Nieprawidlowe dane ESP-NOW");
  }
}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Status wysyłki: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sukces" : "Błąd");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 SLAVE - Suwmiarka + ESP-NOW ===");
  pinMode(DATA_PIN, INPUT_PULLUP);
  pinMode(CLOCK_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  delay(100);

  int attempts = 0;
  while (WiFi.status() == WL_NO_SHIELD && attempts < 10) {
    delay(100);
    attempts++;
  }

  if (attempts < 10) {
    Serial.print("MAC Address Slave: ");
    Serial.println(WiFi.macAddress());
  } else {
    Serial.println("BŁĄD: WiFi nie może się zainicjalizować!");
    return;
  }

  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    Serial.println("BŁĄD ESP-NOW");
    return;
  }
  Serial.println("ESP-NOW OK");

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = ESPNOW_WIFI_CHANNEL;
  peerInfo.encrypt = false;

  int peerTries = 0;
  while (esp_now_add_peer(&peerInfo) != ESP_OK && peerTries < 10) {
    Serial.print("Próba dodania Master... ");
    Serial.println(peerTries + 1);
    delay(100);
    peerTries++;
  }

  if (peerTries < 10) {
    Serial.println("Master dodany jako peer!");
  } else {
    Serial.println("BŁĄD dodania Master! Sprawdź MAC, kanał, zasięg...");
    return;
  }

  Serial.println("Oczekiwanie na żądania pomiaru...\n");
}

void loop() {
  if (measurementRequested) {
    measurementRequested = false;
    float result = performMeasurement();
    sensorData.measurement = result;
    sensorData.valid = (result != -999.0);
    sensorData.timestamp = millis();

    esp_err_t sendResult = esp_now_send(masterAddress, (uint8_t *) &sensorData, sizeof(sensorData));
    if (sendResult == ESP_OK) {
      Serial.println("Wynik wyslany do Mastera");
    } else {
      Serial.print("BLAD wysylania wyniku: ");
      Serial.println(sendResult);

      // Próba ponownego wysłania po krótkiej przerwie
      delay(100);
      sendResult = esp_now_send(masterAddress, (uint8_t *) &sensorData, sizeof(sensorData));
      if (sendResult == ESP_OK) {
        Serial.println("Ponowne wyslanie wyniku udane");
      } else {
        Serial.println("Ponowne wyslanie wyniku nieudane");
      }
    }
    Serial.println("-------------------------------\n");
  }

  // Zamiast delay(10) - lepsza responsywność
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck >= 10) {
    lastCheck = millis();
    // Możliwość dodania innych zadań
  }
}
