/**
 * @file caliper.cpp
 * @brief Caliper (suwmiarka) sensor implementation for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 1.0
 */

#include "caliper.h"

#include <MacroDebugger.h>

// Static member initialization
volatile uint8_t CaliperInterface::bitBuffer[52] = {0};
volatile int CaliperInterface::bitCount = 0;
volatile bool CaliperInterface::dataReady = false;

void IRAM_ATTR CaliperInterface::clockISR()
{
    if (bitCount < 52)
    {
        uint8_t bit = digitalRead(DATA_PIN);
        bitBuffer[bitCount] = bit;
        bitCount = bitCount + 1;
        if (bitCount == 52)
        {
            dataReady = true;
        }
    }
}

void CaliperInterface::reverseBits()
{
    for (int i = 0; i < 26; i++)
    {
        uint8_t temp = bitBuffer[i];
        bitBuffer[i] = bitBuffer[51 - i];
        bitBuffer[51 - i] = temp;
    }
}

float CaliperInterface::decodeCaliper()
{
    uint8_t shifted[52];
    for (int i = 0; i < 52; i++)
    {
        if (i + 8 < 52)
            shifted[i] = bitBuffer[i + 8];
        else
            shifted[i] = 0;
    }
    uint8_t nibbles[13];
    for (int i = 0; i < 13; i++)
    {
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
    if (negative)
        measurement = -measurement;
    if (inchMode)
        measurement *= 25.4;
    return measurement;
}

void CaliperInterface::begin()
{
    pinMode(DATA_PIN, INPUT_PULLUP);
    pinMode(CLOCK_PIN, INPUT_PULLUP);
    pinMode(TRIG_PIN, OUTPUT);
    digitalWrite(TRIG_PIN, HIGH);
}

float CaliperInterface::performMeasurement()
{
    DEBUG_I("Wyzwalam pomiar TRIG...");
    digitalWrite(TRIG_PIN, LOW);

    bitCount = 0;
    dataReady = false;

    attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), clockISR, FALLING);

    unsigned long startTime = millis();
    while (!dataReady && (millis() - startTime < MEASUREMENT_TIMEOUT_MS))
    {
        delayMicroseconds(100);
    }

    detachInterrupt(digitalPinToInterrupt(CLOCK_PIN));
    digitalWrite(TRIG_PIN, HIGH);

    if (dataReady)
    {
        reverseBits();
        float result = decodeCaliper();

        if (result >= MEASUREMENT_MIN_VALUE && result <= MEASUREMENT_MAX_VALUE && !isnan(result) && !isinf(result))
        {
            DEBUG_I(">Pomiar: %.3f mm", result);
            return result;
        }
        else
        {
            DEBUG_E("BŁĄD: Nieprawidłowa wartość pomiaru!");
            return INVALID_MEASUREMENT_VALUE;
        }
    }
    else
    {
        DEBUG_E("BŁĄD: Timeout!");
        return INVALID_MEASUREMENT_VALUE;
    }
}
