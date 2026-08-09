#include "serial_cli.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <MacroDebugger.h>
#include <shared_common.h>
#include <shared_config.h>
#include "preferences_manager.h"
#include "measurement_state.h"

bool parseIntStrict(const String &s, long &out)
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

bool parseFloatStrict(const String &s, float &out)
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

static SerialCliContext g_ctx;

/**
 * @brief Session name validation
 *
 * @param name Session name to validate
 * @return true Name is valid
 * @return false Name is invalid
 */
static bool validateSessionName(const String &name)
{
  // Minimum length: 1 character
  if (name.length() < 1)
  {
    DEBUG_W("Session name is empty");
    return false;
  }

  // Maximum length: 31 characters (32 with null terminator)
  if (name.length() > 31)
  {
    DEBUG_W("Session name is too long (max 31 characters)");
    return false;
  }

  // Character validation: letters (a-z, A-Z), digits (0-9), spaces, underscores (_), hyphens (-)
  for (unsigned int i = 0; i < name.length(); i++)
  {
    char c = name.charAt(i);
    if (!(isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-'))
    {
      DEBUG_W("Session name contains invalid characters: '%c'", c);
      return false;
    }
  }

  return true;
}

static void printSerialHelp()
{
  DEBUG_I("\n=== AVAILABLE SERIAL COMMANDS (UART) ===\n"
          "m            - Send to slave: CMD_MEASURE (M)\n"
          "u            - Send to slave: CMD_UPDATE (U)\n"
          "o <ms>       - Set timeout\n"
          "q <0-255>    - Set motorTorque\n"
          "s <0-255>    - Set motorSpeed\n"
          "r <0-3>      - Set motorState (0=STOP, 1=FORWARD, 2=REVERSE, 3=BRAKE)\n"
          "t            - Send CMD_MOTORTEST (T) with current settings\n"
          "f            - Send CMD_OTA (O) – enter OTA mode on Slave (flash)\n"
          "p            - Pairing mode (30s broadcast CMD_PAIR)\n"
          "c <±999.999> - Set calibrationOffset (mm) on Master (without triggering measurement)\n"
          "v <±999.999>  - Set reference (mm) on Master (reference/nominal value)\n"
          "n <name>     - Set session name (max 31 characters, allowed: a-z, A-Z, 0-9, space, _, -)\n"
          "g            - Refresh settings (send all current values)\n"
          "h/?          - Show this help\n"
          "=====================================\n");
}

void SerialCli_begin(const SerialCliContext &ctx)
{
  g_ctx = ctx;
}

bool SerialCli_tick(void *arg)
{
  (void)arg;

  // Line parser: read until '\n' without blocking.
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
      // Line length limit to avoid RAM overflow from Serial garbage.
      if (lineBuf.length() < 64)
      {
        lineBuf += ch;
      }
      continue;
    }

    // Full line received
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

    if (g_ctx.systemStatus == nullptr)
    {
      DEBUG_E("SerialCli: missing systemStatus (SerialCli_begin not called?)");
      return true;
    }

    switch (cmd)
    {
    case 'm':
      if (g_ctx.requestMeasurement)
      {
        g_ctx.requestMeasurement();
        
        // Log result if measurementState is available
        if (g_ctx.measurementState != nullptr)
        {
          if (g_ctx.measurementState->isReady())
          {
            DEBUG_I("Measurement completed: %s", g_ctx.measurementState->getMeasurement());
          }
          else
          {
            DEBUG_W("Measurement failed or timeout");
          }
        }
      }
      break;

    case 'o':
      if (!parseIntStrict(rest, val))
      {
        DEBUG_W("Serial: missing/invalid parameter for 'o' (use: o <ms>\\n)");
        printSerialHelp();
        break;
      }

      if (val < 0 || val > 600000)
      {
        DEBUG_W("Serial: timeout out of range: %ld (0..600000 ms)", val);
        break;
      }

      g_ctx.systemStatus->msgMaster.timeout = (uint32_t)val;
      DEBUG_I("tx.timeout:%u", (unsigned)g_ctx.systemStatus->msgMaster.timeout);

      // Save to Preferences
      if (g_ctx.prefsManager != nullptr)
      {
        g_ctx.prefsManager->saveTimeout((uint32_t)val);
      }

      // Unify channel for GUI (DEBUG_PLOT) — GUI can update state immediately.
      DEBUG_PLOT("timeout:%u", (unsigned)g_ctx.systemStatus->msgMaster.timeout);
      break;

    case 'u':
      if (g_ctx.requestUpdate)
      {
        g_ctx.requestUpdate();
        
        // Log result if measurementState is available
        if (g_ctx.measurementState != nullptr)
        {
          if (g_ctx.measurementState->isReady())
          {
            DEBUG_I("Status updated: %s, Battery: %s",
                   g_ctx.measurementState->getMeasurement(),
                   g_ctx.measurementState->getBatteryVoltage());
          }
          else
          {
            DEBUG_W("Status update failed or timeout");
          }
        }
      }
      break;

    case 'c':
      if (!parseFloatStrict(rest, fval))
      {
        DEBUG_W("Serial: missing/invalid parameter for 'c' (use: c <offset_mm>\\n)");
        printSerialHelp();
        break;
      }

      if (fval < CALIBRATION_OFFSET_MIN || fval > CALIBRATION_OFFSET_MAX)
      {
        DEBUG_W("Serial: calibrationOffset out of range: %.3f (-999.999..999.999)", (double)fval);
        break;
      }

      g_ctx.systemStatus->calibrationOffset = fval;
      DEBUG_I("calibrationOffset:%.3f", (double)g_ctx.systemStatus->calibrationOffset);

      // Save to Preferences
      if (g_ctx.prefsManager != nullptr)
      {
        g_ctx.prefsManager->saveCalibrationOffset(fval);
      }

      // Unify channel for GUI (DEBUG_PLOT) — GUI can update state immediately.
      DEBUG_PLOT("calibrationOffset:%.3f", (double)g_ctx.systemStatus->calibrationOffset);
      break;

    case 'v':
      if (!parseFloatStrict(rest, fval))
      {
        DEBUG_W("Serial: missing/invalid parameter for 'v' (use: v <reference_mm>\\n)");
        printSerialHelp();
        break;
      }

      if (fval < -999.999f || fval > 999.999f)
      {
        DEBUG_W("Serial: reference out of range: %.3f (-999.999..999.999)", (double)fval);
        break;
      }

      g_ctx.systemStatus->reference = fval;
      DEBUG_I("reference:%.3f", (double)g_ctx.systemStatus->reference);

      // Save to Preferences
      if (g_ctx.prefsManager != nullptr)
      {
        g_ctx.prefsManager->saveReference(fval);
      }

      // Unify channel for GUI (DEBUG_PLOT) — GUI can update state immediately.
      DEBUG_PLOT("reference:%.3f", (double)g_ctx.systemStatus->reference);
      break;

    case 'q':
      if (!parseIntStrict(rest, val))
      {
        DEBUG_W("Serial: missing/invalid parameter for 'q' (use: q <0-255>\\n)");
        printSerialHelp();
        break;
      }

      if (val < 0 || val > 255)
      {
        DEBUG_W("Serial: motorTorque out of range: %ld (0..255)", val);
        break;
      }

      g_ctx.systemStatus->msgMaster.motorTorque = (uint8_t)val;
      DEBUG_I("tx.motorTorque:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorTorque);

      // Save to Preferences
      if (g_ctx.prefsManager != nullptr)
      {
        g_ctx.prefsManager->saveMotorTorque((uint8_t)val);
      }

      // Unify channel for GUI (DEBUG_PLOT) — GUI can update state immediately.
      DEBUG_PLOT("motorTorque:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorTorque);
      break;

    case 's':
      if (!parseIntStrict(rest, val))
      {
        DEBUG_W("Serial: missing/invalid parameter for 's' (use: s <0-255>\\n)");
        printSerialHelp();
        break;
      }

      if (val < 0 || val > 255)
      {
        DEBUG_W("Serial: motorSpeed out of range: %ld (0..255)", val);
        break;
      }

      g_ctx.systemStatus->msgMaster.motorSpeed = (uint8_t)val;
      DEBUG_I("tx.motorSpeed:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorSpeed);

      // Save to Preferences
      if (g_ctx.prefsManager != nullptr)
      {
        g_ctx.prefsManager->saveMotorSpeed((uint8_t)val);
      }

      // Unify channel for GUI (DEBUG_PLOT) — GUI can update state immediately.
      DEBUG_PLOT("motorSpeed:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorSpeed);
      break;

    case 'r':
      if (!parseIntStrict(rest, val))
      {
        DEBUG_W("Serial: missing/invalid parameter for 'r' (use: r <0-3>\\n)");
        printSerialHelp();
        break;
      }

      if (val < 0 || val > 3)
      {
        DEBUG_W("Serial: motorState out of range: %ld (0..3)", val);
        break;
      }

      g_ctx.systemStatus->msgMaster.motorState = (MotorState)val;
      DEBUG_I("tx.motorState:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorState);

      // Unify channel for GUI (DEBUG_PLOT) — GUI can update state immediately.
      DEBUG_PLOT("motorState:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorState);
      break;

    case 't':
      if (g_ctx.sendMotorTest)
      {
        g_ctx.sendMotorTest();
      }
      break;

    case 'f':
      if (g_ctx.sendOTA)
      {
        g_ctx.sendOTA();
        DEBUG_I("CMD_OTA sent – Slave will enter OTA mode");
      }
      break;

    case 'p':
      if (g_ctx.enterPairingMode)
      {
        g_ctx.enterPairingMode();
      }
      break;

    case 'n':
      // Set session name
      if (!validateSessionName(rest))
      {
        DEBUG_W("Serial: invalid session name for 'n' (use: n <name>\\n)");
        printSerialHelp();
        break;
      }

      // Save session name to systemStatus.sessionName
      memset(g_ctx.systemStatus->sessionName, 0, sizeof(g_ctx.systemStatus->sessionName));
      strncpy(g_ctx.systemStatus->sessionName, rest.c_str(), sizeof(g_ctx.systemStatus->sessionName) - 1);
      
      // Unify channel for GUI (DEBUG_PLOT) — GUI can update state immediately.
      DEBUG_PLOT("sessionName:%s", g_ctx.systemStatus->sessionName);
      break;

    case 'g':
      // Send all current settings via DEBUG_PLOT
      DEBUG_PLOT("calibrationOffset:%.3f", (double)g_ctx.systemStatus->calibrationOffset);
      DEBUG_PLOT("reference:%.3f", (double)g_ctx.systemStatus->reference);
      DEBUG_PLOT("timeout:%u", (unsigned)g_ctx.systemStatus->msgMaster.timeout);
      DEBUG_PLOT("motorTorque:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorTorque);
      DEBUG_PLOT("motorSpeed:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorSpeed);
      DEBUG_PLOT("motorState:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorState);
      DEBUG_PLOT("sessionName:%s", g_ctx.systemStatus->sessionName);
      break;

    case 'h':
    case '?':
      printSerialHelp();
      break;

    default:
      DEBUG_W("Serial: unknown command: '%c' (line: %s)", cmd, line.c_str());
      printSerialHelp();
      break;
    }
  }

  return true;
}
