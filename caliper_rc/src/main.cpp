#include <esp_now.h>
#include <WiFi.h>
#include "config.h"
#include <shared_common.h>
#include <error_handler.h>
#include <MacroDebugger.h>
#include <arduino-timer.h>
#include "communication.h"

uint8_t masterAddress[] = MASTER_MAC_ADDR;

CommunicationManager commManager;

static bool lastTrigState = HIGH;
static bool lastDropState = HIGH;
static unsigned long lastTrigDebounce = 0;
static unsigned long lastDropDebounce = 0;
static bool trigPressed = false;
static bool dropPressed = false;

static constexpr unsigned long LED_PULSE_MS = 100;
static unsigned long ledOnTime = 0;

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  (void)recv_info;
  (void)incomingData;
  (void)len;
}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  (void)info;
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    DEBUG_I("ESP-NOW send: OK");
  }
  else
  {
    RECORD_ERROR(ERR_ESPNOW_SEND_FAILED, "ESP-NOW send callback reported failure");
  }
}

static void sendRCCommand(CommandType cmd)
{
  MessageRC msg{};
  msg.command = cmd;

  ErrorCode result = commManager.sendMessage(msg);
  if (result == ERR_NONE)
  {
    DEBUG_I("Wyslano komende RC: %c", (char)cmd);
  }
  else
  {
    DEBUG_E("Blad wysylki RC: %c (err=0x%04X)", (char)cmd, result);
  }

  digitalWrite(LED_PIN, HIGH);
  ledOnTime = millis();
}

static void handleButtons()
{
  bool trigReading = digitalRead(BUTTON_TRIG_PIN);
  bool dropReading = digitalRead(BUTTON_DROP_PIN);

  unsigned long now = millis();

  if (trigReading != lastTrigState)
  {
    lastTrigDebounce = now;
  }

  if (dropReading != lastDropState)
  {
    lastDropDebounce = now;
  }

  if ((now - lastTrigDebounce) > DEBOUNCE_DELAY_MS)
  {
    if (trigReading == LOW && !trigPressed)
    {
      trigPressed = true;
      sendRCCommand(CMD_TRIG_MEAS);
    }
    else if (trigReading == HIGH)
    {
      trigPressed = false;
    }
  }

  if ((now - lastDropDebounce) > DEBOUNCE_DELAY_MS)
  {
    if (dropReading == LOW && !dropPressed)
    {
      dropPressed = true;
      sendRCCommand(CMD_DROP_MEAS);
    }
    else if (dropReading == HIGH)
    {
      dropPressed = false;
    }
  }

  lastTrigState = trigReading;
  lastDropState = dropReading;

  if (ledOnTime > 0 && (now - ledOnTime) > LED_PULSE_MS)
  {
    digitalWrite(LED_PIN, LOW);
    ledOnTime = 0;
  }
}

void setup()
{
  DEBUG_BEGIN();
  DEBUG_I("=== ESP32 RC - Suwmiarka + ESP-NOW ===");

  ERROR_HANDLER.initialize();

  pinMode(BUTTON_TRIG_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DROP_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
  DEBUG_I("MAC Address RC: %s", WiFi.macAddress().c_str());

  ErrorCode commResult = commManager.initialize(masterAddress);
  if (commResult != ERR_NONE)
  {
    LOG_ERROR(commResult, "Failed to initialize ESP-NOW communication");
    return;
  }

  commManager.setReceiveCallback(OnDataRecv);
  commManager.setSendCallback(OnDataSent);

  DEBUG_I("RC gotowy. Przycisk TRIG=GPIO%d, DROP=GPIO%d", BUTTON_TRIG_PIN, BUTTON_DROP_PIN);
}

void loop()
{
  handleButtons();
}
