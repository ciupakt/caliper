#include <esp_now.h>
#include <WiFi.h>
#include <Preferences.h>
#include "config.h"
#include <shared_common.h>
#include <error_handler.h>
#include <MacroDebugger.h>
#include <espnow_helper.h>
#include <arduino-timer.h>
#include "communication.h"

uint8_t masterAddress[] = MASTER_MAC_ADDR;

Preferences rcPrefs;
CommunicationManager commManager;

static bool lastTrigState = HIGH;
static bool lastDropState = HIGH;
static unsigned long lastTrigDebounce = 0;
static unsigned long lastDropDebounce = 0;
static bool trigPressed = false;
static bool dropPressed = false;

static constexpr unsigned long LED_PULSE_MS = 100;
static unsigned long ledOnTime = 0;

static bool pairingMode = false;
static uint32_t pairingModeStartMs = 0;
static bool hasStoredMasterMac = false;

static bool isMacUnset(const uint8_t mac[6])
{
  for (int i = 0; i < 6; i++)
  {
    if (mac[i] != 0x00) return false;
  }
  return true;
}

static void enterPairingMode()
{
  pairingMode = true;
  pairingModeStartMs = millis();

  esp_now_peer_info_t broadcastPeer{};
  uint8_t broadcastAddr[] = BROADCAST_MAC_ADDR;
  memcpy(broadcastPeer.peer_addr, broadcastAddr, 6);
  broadcastPeer.channel = ESPNOW_WIFI_CHANNEL;
  broadcastPeer.encrypt = false;
  esp_now_add_peer(&broadcastPeer);

  DEBUG_I("RC: tryb parowania aktywny");
}

static void exitPairingMode()
{
  pairingMode = false;

  uint8_t broadcastAddr[] = BROADCAST_MAC_ADDR;
  esp_now_del_peer(broadcastAddr);

  DEBUG_I("RC: tryb parowania zakończony");
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
  uint8_t src_addr[6];
  memcpy(src_addr, recv_info->src_addr, 6);

  if (pairingMode && len == sizeof(MessageMaster))
  {
    MessageMaster tmpMsg{};
    memcpy(&tmpMsg, incomingData, sizeof(tmpMsg));

    if (tmpMsg.command == CMD_PAIR)
    {
      commManager.updatePeerAddress(src_addr);
      memcpy(masterAddress, src_addr, 6);

      rcPrefs.putBytes("masterMac", src_addr, 6);

      MessageRC pairResp{};
      pairResp.command = CMD_PAIR;
      commManager.sendMessage(pairResp);

      DEBUG_I("Otrzymano CMD_PAIR od Mastera: %02X:%02X:%02X:%02X:%02X:%02X",
        src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5]);
      return;
    }

    if (tmpMsg.command == CMD_PAIR_ACK)
    {
      rcPrefs.putBytes("masterMac", src_addr, 6);
      hasStoredMasterMac = true;
      exitPairingMode();
      DEBUG_I("RC: Parowanie zakończone");
      return;
    }
  }
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

  rcPrefs.begin("caliper_rc", false);

  uint8_t storedMasterMac[6];
  memset(storedMasterMac, 0, 6);
  rcPrefs.getBytes("masterMac", storedMasterMac, 6);

  if (!isMacUnset(storedMasterMac))
  {
    memcpy(masterAddress, storedMasterMac, 6);
    hasStoredMasterMac = true;
    DEBUG_I("Master MAC z NVS: %02X:%02X:%02X:%02X:%02X:%02X",
      masterAddress[0], masterAddress[1], masterAddress[2], masterAddress[3], masterAddress[4], masterAddress[5]);
  }
  else
  {
    DEBUG_W("Brak Master MAC w NVS — używam fallback z config.h");
    hasStoredMasterMac = false;
  }

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

  enterPairingMode();

  DEBUG_I("RC gotowy. Przycisk TRIG=GPIO%d, DROP=GPIO%d", BUTTON_TRIG_PIN, BUTTON_DROP_PIN);
}

void loop()
{
  if (pairingMode)
  {
    uint32_t elapsed = millis() - pairingModeStartMs;
    if (hasStoredMasterMac && elapsed >= PAIRING_WINDOW_MS)
    {
      exitPairingMode();
    }
  }

  handleButtons();
}
