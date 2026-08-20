/**
 * @file rs485.cpp
 * @brief RS485 (MAX485) sensor implementation for ESP32
 * @author System Generated
 * @date 2026-08-14
 * @version 1.0
 *
 * @details
 * Implements the RS485 ASCII interface for a Sylvac S_Probe P12D probe
 * connected through a MAX485 transceiver. See rs485.h for the protocol
 * summary and doc/Quickstart-guide-S_Probe-P12D-Open_WEB.pdf for the
 * full specification.
 *
 * @version 1.0 - Initial implementation for RS485 ASCII interface
 */

#if defined(RS485)

#include "rs485.h"

#include <MacroDebugger.h>
#include <error_handler.h>
#include <stdlib.h>

void RS485Interface::setTransmitMode()
{
    digitalWrite(RS485_DE_RE_PIN, HIGH);
}

void RS485Interface::setReceiveMode()
{
    digitalWrite(RS485_DE_RE_PIN, LOW);
}

bool RS485Interface::sendQuery()
{
    setTransmitMode();

    Serial1.write(RS485_QUERY_CHAR);
    Serial1.write(RS485_CR);
    Serial1.flush();

    // Guard delay (~1 char time at 115200 Bd ~= 87 us) to ensure the last
    // stop bit has been shifted out before releasing the bus.
    delayMicroseconds(100);

    setReceiveMode();
    return true;
}

float RS485Interface::readResponse()
{
    char buffer[RS485_RESPONSE_BUFFER_SIZE];
    size_t index = 0;

    unsigned long startTime = millis();
    while (millis() - startTime < RS485_RESPONSE_TIMEOUT_MS)
    {
        while (Serial1.available())
        {
            char c = (char)Serial1.read();

            if (c == RS485_CR || c == '\n')
            {
                buffer[index] = '\0';
                if (index > 0)
                {
                    char *endPtr = nullptr;
                    float value = strtof(buffer, &endPtr);
                    if (endPtr != buffer)
                    {
                        return value;
                    }
                    else
                    {
                        RECORD_ERROR(ERR_RS485_INVALID_RESPONSE, "Unparseable response: '%s'", buffer);
                        return INVALID_MEASUREMENT_VALUE;
                    }
                }
                // Empty line before terminator - keep waiting
            }
            else if (index < RS485_RESPONSE_BUFFER_SIZE - 1)
            {
                buffer[index++] = c;
            }
        }
        delay(POLL_DELAY_MS);
    }

    buffer[index] = '\0';
    if (index > 0)
    {
        char *endPtr = nullptr;
        float value = strtof(buffer, &endPtr);
        if (endPtr != buffer)
        {
            return value;
        }
        RECORD_ERROR(ERR_RS485_INVALID_RESPONSE, "Unparseable response: '%s'", buffer);
    }
    else
    {
        RECORD_ERROR(ERR_RS485_TIMEOUT, "No response within %u ms", RS485_RESPONSE_TIMEOUT_MS);
    }
    return INVALID_MEASUREMENT_VALUE;
}

void RS485Interface::begin()
{
    pinMode(RS485_DE_RE_PIN, OUTPUT);
    setReceiveMode();

    Serial1.begin(RS485_BAUD_RATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    Serial1.flush();

    DEBUG_I("RS485 initialized: %u Bd, 8N1, TX=GPIO%d RX=GPIO%d DE/RE=GPIO%d",
        (unsigned)RS485_BAUD_RATE, RS485_TX_PIN, RS485_RX_PIN, RS485_DE_RE_PIN);
}

/**
 * @brief Performs a single RS485 measurement
 *
 * Sends the '?' position query to the probe over the RS485 bus, reads the
 * ASCII response, parses it into a numeric value, and validates the result.
 *
 * @details
 * Operation flow:
 * 1. Send query:
 *    - Switch MAX485 to transmit mode (DE/RE HIGH)
 *    - Write '?' + CR (0x0D)
 *    - Flush TX FIFO and wait for last bit to shift out
 *    - Switch MAX485 to receive mode (DE/RE LOW)
 *
 * 2. Read response:
 *    - Read ASCII characters until CR/LF or timeout
 *    - Parse numeric value with strtof
 *
 * 3. Result validation:
 *    - Range check: MEASUREMENT_MIN_VALUE to MEASUREMENT_MAX_VALUE
 *    - NaN / Inf check
 *    - On error -> return INVALID_MEASUREMENT_VALUE
 *
 * Notes:
 * - This function is blocking - waits for response or timeout
 * - The probe replies in ASCII mode at 115200 Bd, 8N1
 *
 * @return Measurement value in millimeters or INVALID_MEASUREMENT_VALUE on error
 */
float RS485Interface::performMeasurement()
{
    DEBUG_I("Triggering RS485 measurement...");

    sendQuery();

    float result = readResponse();
    if (result == INVALID_MEASUREMENT_VALUE)
    {
        return INVALID_MEASUREMENT_VALUE;
    }

    if (result >= MEASUREMENT_MIN_VALUE && result <= MEASUREMENT_MAX_VALUE && !isnan(result) && !isinf(result))
    {
        DEBUG_I("Measurement: %.3f mm", result);
        return result;
    }
    else
    {
        RECORD_ERROR(ERR_RS485_OUT_OF_RANGE, "Measurement value: %.3f (range: %.1f to %.1f)",
            result, MEASUREMENT_MIN_VALUE, MEASUREMENT_MAX_VALUE);
        return INVALID_MEASUREMENT_VALUE;
    }
}

float RS485Interface::performReliableMeasurement()
{
    return performMeasurement();
}

#endif // defined(RS485)
