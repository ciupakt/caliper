/**
 * @file caliper.cpp
 * @brief Caliper sensor implementation for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 2.0
 *
 * @version 2.0 - Integrated comprehensive error code system
 */

#include "caliper.h"

#include <MacroDebugger.h>
#include <error_handler.h>

// Static member initialization
volatile uint8_t CaliperInterface::bitBuffer[CALIPER_BIT_BUFFER_SIZE] = {0};
volatile int CaliperInterface::bitCount = 0;
volatile bool CaliperInterface::dataReady = false;
volatile uint8_t bit;

/**
 * @brief Interrupt Service Routine (ISR) for caliper clock signal
 *
 * This function is called automatically on each falling edge
 * of the clock signal (CLOCK_PIN). Reads a bit from the data line
 * and stores it to the buffer.
 *
 * @details
 * - ISR runs in IRAM (Instruction RAM) for maximum performance
 * - Reads bit from DATA_PIN on each clock edge
 * - Stores bits to bitBuffer[0..51] (52 bits total)
 * - Sets dataReady flag after receiving all bits
 *
 * Caliper data format:
 * - 52 bits total (including header)
 * - First 8 bits are header (ignored in decoding)
 * - Next 44 bits are measurement data
 *
 * Note: ISR should be as short as possible, therefore it does not
 * perform any decoding operations - only stores bits.
 */
void IRAM_ATTR CaliperInterface::clockISR()
{
    bit = digitalRead(CLOCK_PIN);
    if(bit == HIGH) return; // Only process on falling edge

    if (bitCount < CALIPER_BIT_BUFFER_SIZE)
    {
        bit = digitalRead(DATA_PIN);
        bit = digitalRead(DATA_PIN);
        bit = digitalRead(DATA_PIN);

        bitBuffer[bitCount] = bit;
        bitCount = bitCount + 1;
        if (bitCount == CALIPER_BIT_BUFFER_SIZE)
        {
            dataReady = true;
        }
    }
}

/**
 * @brief Reverses the bit order in the buffer
 *
 * This function reverses the bit order received from the caliper.
 * This is necessary because the caliper protocol sends bits
 * in reverse order to what the decoder expects.
 *
 * @details
 * Algorytm:
 * - Iterates through the first half of the buffer (0 to 25)
 * - Zamienia miejscami bit[i] z bit[51-i]
 * - Po zamianie: bit[0] ↔ bit[51], bit[1] ↔ bit[50], itd.
 *
 * Example for 8 bits:
 * Before: [0, 1, 0, 1, 1, 0, 0, 1]
 * Po:     [1, 0, 0, 1, 1, 0, 1, 0]
 *
 * Note: This function operates on bitBuffer, which is volatile,
 * but is safe because it is called after ISR completes
 * (when interrupts are detached).
 */
void CaliperInterface::reverseBits()
{
    for (int i = 0; i < CALIPER_BIT_BUFFER_SIZE / 2; i++)
    {
        uint8_t temp = bitBuffer[i];
        bitBuffer[i] = bitBuffer[CALIPER_BIT_BUFFER_SIZE - 1 - i];
        bitBuffer[CALIPER_BIT_BUFFER_SIZE - 1 - i] = temp;
    }
}

/**
 * @brief Decodes caliper data to value in millimeters
 *
 * This function decodes the raw bits received from the caliper and converts
 * them into a measurement value in millimeters.
 *
 * @details
 * Caliper data format (after bit reversal):
 * - 52 bits total
 * - First 8 bits (0-7): header (ignored)
 * - Next 44 bits (8-51): measurement data
 *
 * Step 1: Bit shift
 * - Removes 8-bit header
 * - shifted[0] = bitBuffer[8], shifted[1] = bitBuffer[9], itd.
 *
 * Step 2: Grouping into nibbles (4 bits)
 * - 44 bity = 11 nibbli po 4 bity
 * - Each nibble represents one BCD digit
 * - nibbles[0..4]: decimal digits (0-9)
 * - nibbles[5]: units (0-9)
 * - nibbles[6]: flags (bit 2: inch mode, bit 3: negative)
 *
 * Step 3: BCD decoding
 * - BCD (Binary Coded Decimal): each digit is encoded as 4 bits
 * - Value = Σ(nibbles[i] × 10^i) for i = 0..4
 * - Example: nibbles = [5, 4, 3, 2, 1] → 1×10⁴ + 2×10³ + 3×10² + 4×10¹ + 5×10⁰ = 12345
 *
 * Step 4: Flag handling
 * - negative (nibbles[6] & 0x08): if true, value is negative
 * - inchMode (nibbles[6] & 0x04): if true, value is in inches
 *
 * Step 5: Unit conversion
 * - Dividing by CALIPER_VALUE_DIVISOR (1000.0) converts to millimeters
 * - If inchMode: multiplication by INCH_TO_MM_FACTOR (25.4) converts inches → mm
 *
 * Examples:
 * - nibbles = [5, 4, 3, 2, 1, 0, 0x00] → 123.456 mm
 * - nibbles = [5, 4, 3, 2, 1, 0, 0x04] → 123.456 mm (inch mode, but already in mm)
 * - nibbles = [5, 4, 3, 2, 1, 0, 0x08] → -123.456 mm
 *
 * @return Measurement value in millimeters
 */
float CaliperInterface::decodeCaliper()
{
    uint8_t shifted[CALIPER_BIT_BUFFER_SIZE];
    for (int i = 0; i < CALIPER_BIT_BUFFER_SIZE; i++)
    {
        if (i + CALIPER_BIT_SHIFT < CALIPER_BIT_BUFFER_SIZE)
            shifted[i] = bitBuffer[i + CALIPER_BIT_SHIFT];
        else
            shifted[i] = 0;
    }
    uint8_t nibbles[CALIPER_NIBBLE_COUNT];
    for (int i = 0; i < CALIPER_NIBBLE_COUNT; i++)
    {
        nibbles[i] = 0;
        for (int j = 0; j < BITS_PER_NIBBLE; j++)
            nibbles[i] |= (shifted[i * BITS_PER_NIBBLE + (BITS_PER_NIBBLE - 1 - j)] << j);
    }
    long value = 0;
    for (int i = 0; i < CALIPER_DECIMAL_DIGITS; i++)
        value += nibbles[i] * pow(10, i);
    bool negative = nibbles[6] & 0x08;
    bool inchMode = nibbles[6] & 0x04;
    float measurement = value / CALIPER_VALUE_DIVISOR;
    if (negative)
        measurement = -measurement;
    if (inchMode)
        measurement *= INCH_TO_MM_FACTOR;
    return measurement;
}

void CaliperInterface::begin()
{
    pinMode(DATA_PIN, INPUT_PULLUP);
    pinMode(CLOCK_PIN, INPUT_PULLUP);
    pinMode(TRIG_PIN, OUTPUT);
    digitalWrite(TRIG_PIN, HIGH);
}

/**
 * @brief Performs a single caliper measurement
 *
 * This function triggers a measurement, waits for data reception, decodes it
 * and returns the value in millimeters.
 *
 * @details
 * Operation flow:
 *
 * 1. Trigger measurement:
 *    - TRIG_PIN → LOW (activates caliper)
 *    - Caliper starts sending data via CLOCK_PIN
 *
 * 2. Waiting for data:
 *    - Reset bit counter (bitCount = 0)
 *    - Reset ready flag (dataReady = false)
 *    - Attach ISR to CLOCK_PIN (falling edge)
 *    - ISR (clockISR) reads bits to bitBuffer
 *
 * 3. Timeout:
 *    - Maximum time: MEASUREMENT_TIMEOUT_MS (200ms)
 *    - If dataReady not set → timeout
 *    - Loop checks flag every POLL_DELAY_MS (1ms)
 *
 * 4. End measurement:
 *    - Detach ISR from CLOCK_PIN
 *    - TRIG_PIN → HIGH (deactivates caliper)
 *
 * 5. Decoding (if dataReady):
 *    - Reverse bit order (reverseBits)
 *    - BCD decoding (decodeCaliper)
 *
 * 6. Result validation:
 *    - Range check: MEASUREMENT_MIN_VALUE to MEASUREMENT_MAX_VALUE
 *    - NaN check (Not a Number)
 *    - Inf check (Infinity)
 *    - On error → return INVALID_MEASUREMENT_VALUE
 *
 * 7. Returning result:
 *    - Success: value in millimeters
 *    - Error (timeout/invalid): INVALID_MEASUREMENT_VALUE
 *
 * Notes:
 * - This function is blocking - waits for timeout or data reception
 * - ISR runs in IRAM for maximum performance
 * - Timeout is necessary to prevent hang when no data is received
 *
 * @return Measurement value in millimeters or INVALID_MEASUREMENT_VALUE on error
 */
float CaliperInterface::performMeasurement()
{
    DEBUG_I("Triggering measurement TRIG...");
    digitalWrite(TRIG_PIN, LOW);

    bitCount = 0;
    dataReady = false;

    attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), clockISR, FALLING);

    unsigned long startTime = millis();
    while (!dataReady && (millis() - startTime < MEASUREMENT_TIMEOUT_MS))
    {
        delay(POLL_DELAY_MS);
    }

    detachInterrupt(digitalPinToInterrupt(CLOCK_PIN));
    digitalWrite(TRIG_PIN, HIGH);

    if (dataReady)
    {
        reverseBits();
        float result = decodeCaliper();

        if (result >= MEASUREMENT_MIN_VALUE && result <= MEASUREMENT_MAX_VALUE && !isnan(result) && !isinf(result))
        {
            DEBUG_I("Measurement: %.3f mm", result);
            return result;
        }
        else
        {
            RECORD_ERROR(ERR_CALIPER_INVALID_DATA, "Measurement value: %.3f (range: %.1f to %.1f)",
                result, MEASUREMENT_MIN_VALUE, MEASUREMENT_MAX_VALUE);
            return INVALID_MEASUREMENT_VALUE;
        }
    }
    else
    {
        RECORD_ERROR(ERR_CALIPER_TIMEOUT, "Timeout after %u ms", MEASUREMENT_TIMEOUT_MS);
        return INVALID_MEASUREMENT_VALUE;
    }
}

/**
 * @brief Perform a reliable measurement with two consecutive identical readings
 *
 * Calls performMeasurement() repeatedly and accepts the result only when two
 * consecutive readings return the same value, to filter out sporadic bad
 * readings from the caliper.
 *
 * @details
 * Loop behavior:
 * - Each iteration calls performMeasurement().
 * - A reading equal to INVALID_MEASUREMENT_VALUE is treated as a failed
 *   attempt and does not count as a candidate; the loop continues.
 * - A valid reading is compared with the last valid reading. If they match,
 *   the value is returned as the reliable result.
 * - The whole procedure is bounded by RELIABLE_MEASUREMENT_TIMEOUT_MS (1s).
 * - On timeout (no two consecutive identical readings), returns
 *   INVALID_MEASUREMENT_VALUE and records ERR_CALIPER_INVALID_DATA.
 *
 * @return Measurement value in millimeters or INVALID_MEASUREMENT_VALUE on error
 */
float CaliperInterface::performReliableMeasurement()
{
    unsigned long startTime = millis();
    float lastValid = INVALID_MEASUREMENT_VALUE;

    while (millis() - startTime < RELIABLE_MEASUREMENT_TIMEOUT_MS)
    {
        float current = performMeasurement();

        if (current == INVALID_MEASUREMENT_VALUE)
        {
            continue;
        }

        if (lastValid != INVALID_MEASUREMENT_VALUE && current == lastValid)
        {
            DEBUG_I("Reliable measurement: %.3f mm", current);
            return current;
        }

        lastValid = current;
    }

    RECORD_ERROR(ERR_CALIPER_INVALID_DATA,
        "Reliable measurement failed: no two consecutive identical readings within %u ms",
        RELIABLE_MEASUREMENT_TIMEOUT_MS);
    return INVALID_MEASUREMENT_VALUE;
}
