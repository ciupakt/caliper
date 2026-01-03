#include "serial_cli.h"

#include <stdlib.h>

#include <MacroDebugger.h>
#include <shared_common.h>

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

static void printSerialHelp()
{
  DEBUG_I("\n=== DOSTĘPNE KOMENDY SERIAL (UART) ===\n"
          "m            - Wyślij do slave: CMD_MEASURE (M)\n"
          "u            - Wyślij do slave: CMD_UPDATE (U)\n"
          "o <ms>       - Ustaw timeout (timeout)\n"
          "q <0-255>    - Ustaw motorTorque\n"
          "s <0-255>    - Ustaw motorSpeed\n"
          "r <0-3>      - Ustaw motorState (0=STOP, 1=FORWARD, 2=REVERSE, 3=BRAKE)\n"
          "t            - Wyślij CMD_MOTORTEST (T) z bieżącymi ustawieniami\n"
          "c <±14.999>  - Ustaw calibrationOffset (mm) na Master (bez wyzwalania pomiaru)\n"
          "h/?          - Wyświetl tę pomoc\n"
          "=====================================\n");
}

void SerialCli_begin(const SerialCliContext &ctx)
{
  g_ctx = ctx;
}

bool SerialCli_tick(void *arg)
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

    if (g_ctx.systemStatus == nullptr)
    {
      DEBUG_E("SerialCli: brak systemStatus (SerialCli_begin nie wywołane?)");
      return true;
    }

    switch (cmd)
    {
    case 'm':
      if (g_ctx.requestMeasurement)
      {
        g_ctx.requestMeasurement();
      }
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

      g_ctx.systemStatus->msgMaster.timeout = (uint32_t)val;
      DEBUG_I("tx.timeout:%u", (unsigned)g_ctx.systemStatus->msgMaster.timeout);

      // Ujednolicamy kanał dla GUI (DEBUG_PLOT) — GUI może od razu zaktualizować stan.
      DEBUG_PLOT("timeout:%u", (unsigned)g_ctx.systemStatus->msgMaster.timeout);
      break;

    case 'u':
      if (g_ctx.requestUpdate)
      {
        g_ctx.requestUpdate();
      }
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

      g_ctx.systemStatus->calibrationOffset = fval;
      DEBUG_I("calibrationOffset:%.3f", (double)g_ctx.systemStatus->calibrationOffset);

      // Ujednolicamy kanał dla GUI (DEBUG_PLOT) — GUI może od razu zaktualizować stan.
      DEBUG_PLOT("calibrationOffset:%.3f", (double)g_ctx.systemStatus->calibrationOffset);
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

      g_ctx.systemStatus->msgMaster.motorTorque = (uint8_t)val;
      DEBUG_I("tx.motorTorque:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorTorque);

      // Ujednolicamy kanał dla GUI (DEBUG_PLOT) — GUI może od razu zaktualizować stan.
      DEBUG_PLOT("motorTorque:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorTorque);
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

      g_ctx.systemStatus->msgMaster.motorSpeed = (uint8_t)val;
      DEBUG_I("tx.motorSpeed:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorSpeed);

      // Ujednolicamy kanał dla GUI (DEBUG_PLOT) — GUI może od razu zaktualizować stan.
      DEBUG_PLOT("motorSpeed:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorSpeed);
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

      g_ctx.systemStatus->msgMaster.motorState = (MotorState)val;
      DEBUG_I("tx.motorState:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorState);

      // Ujednolicamy kanał dla GUI (DEBUG_PLOT) — GUI może od razu zaktualizować stan.
      DEBUG_PLOT("motorState:%u", (unsigned)g_ctx.systemStatus->msgMaster.motorState);
      break;

    case 't':
      if (g_ctx.sendMotorTest)
      {
        g_ctx.sendMotorTest();
      }
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
