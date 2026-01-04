/**
 * @file caliper.cpp
 * @brief Caliper (suwmiarka) sensor implementation for ESP32
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

/**
 * @brief Interrupt Service Routine (ISR) dla sygnału zegara suwmiarki
 *
 * Funkcja jest wywoływana automatycznie na każdym zboczu opadającym
 * sygnału zegara (CLOCK_PIN). Odczytuje bit z linii danych
 * i zapisuje go do bufora.
 *
 * @details
 * - ISR działa w IRAM (Instruction RAM) dla maksymalnej wydajności
 * - Odczytuje bit z DATA_PIN na każdym zboczu zegara
 * - Zapisuje bity do bitBuffer[0..51] (łącznie 52 bity)
 * - Ustawia flagę dataReady po odebraniu wszystkich bitów
 *
 * Format danych suwmiarki:
 * - 52 bity łącznie (w tym nagłówek)
 * - Pierwsze 8 bitów to nagłówek (ignorowany w dekodowaniu)
 * - Kolejne 44 bity to dane pomiarowe
 *
 * Uwaga: ISR powinna być jak najkrótsza, dlatego nie wykonuje
 * żadnych operacji dekodowania - tylko zapisuje bity.
 */
void IRAM_ATTR CaliperInterface::clockISR()
{
    if (bitCount < CALIPER_BIT_BUFFER_SIZE)
    {
        uint8_t bit = digitalRead(DATA_PIN);
        bitBuffer[bitCount] = bit;
        bitCount = bitCount + 1;
        if (bitCount == CALIPER_BIT_BUFFER_SIZE)
        {
            dataReady = true;
        }
    }
}

/**
 * @brief Odwraca kolejność bitów w buforze
 *
 * Funkcja odwraca kolejność bitów odebranych od suwmiarki.
 * Jest to konieczne, ponieważ protokół suwmiarki przesyła bity
 * w kolejności odwrotnej do oczekiwanej przez dekoder.
 *
 * @details
 * Algorytm:
 * - Iteruje przez pierwszą połowę bufora (0 do 25)
 * - Zamienia miejscami bit[i] z bit[51-i]
 * - Po zamianie: bit[0] ↔ bit[51], bit[1] ↔ bit[50], itd.
 *
 * Przykład dla 8 bitów:
 * Przed:  [0, 1, 0, 1, 1, 0, 0, 1]
 * Po:     [1, 0, 0, 1, 1, 0, 1, 0]
 *
 * Uwaga: Funkcja operuje na bitBuffer, który jest volatile,
 * ale jest bezpieczna, ponieważ jest wywoływana po zakończeniu ISR
 * (gdy przerwania są odłączone).
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
 * @brief Dekoduje dane suwmiarki do wartości w milimetrach
 *
 * Funkcja dekoduje surowe bity odebrane od suwmiarki i konwertuje
 * je na wartość pomiarową w milimetrach.
 *
 * @details
 * Format danych suwmiarki (po odwróceniu bitów):
 * - 52 bity łącznie
 * - Pierwsze 8 bitów (0-7): nagłówek (ignorowany)
 * - Kolejne 44 bity (8-51): dane pomiarowe
 *
 * Krok 1: Przesunięcie bitów (shift)
 * - Usuwa 8-bitowy nagłówek
 * - shifted[0] = bitBuffer[8], shifted[1] = bitBuffer[9], itd.
 *
 * Krok 2: Grupowanie w nibble (4 bity)
 * - 44 bity = 11 nibbli po 4 bity
 * - Każdy nibble reprezentuje jedną cyfrę BCD
 * - nibbles[0..4]: cyfry dziesiętne (0-9)
 * - nibbles[5]: jednostki (0-9)
 * - nibbles[6]: flagi (bit 2: inch mode, bit 3: negative)
 *
 * Krok 3: Dekodowanie BCD
 * - BCD (Binary Coded Decimal): każda cyfra jest kodowana jako 4 bity
 * - Wartość = Σ(nibbles[i] × 10^i) dla i = 0..4
 * - Przykład: nibbles = [5, 4, 3, 2, 1] → 1×10⁴ + 2×10³ + 3×10² + 4×10¹ + 5×10⁰ = 12345
 *
 * Krok 4: Obsługa flag
 * - negative (nibbles[6] & 0x08): jeśli true, wartość jest ujemna
 * - inchMode (nibbles[6] & 0x04): jeśli true, wartość jest w calach
 *
 * Krok 5: Konwersja jednostek
 * - Dzielenie przez CALIPER_VALUE_DIVISOR (1000.0) konwertuje na milimetry
 * - Jeśli inchMode: mnożenie przez INCH_TO_MM_FACTOR (25.4) konwertuje cale → mm
 *
 * Przykłady:
 * - nibbles = [5, 4, 3, 2, 1, 0, 0x00] → 123.456 mm
 * - nibbles = [5, 4, 3, 2, 1, 0, 0x04] → 123.456 mm (inch mode, ale już w mm)
 * - nibbles = [5, 4, 3, 2, 1, 0, 0x08] → -123.456 mm
 *
 * @return Wartość pomiaru w milimetrach
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
 * @brief Wykonuje pojedynczy pomiar suwmiarką
 *
 * Funkcja wyzwala pomiar, czeka na odebranie danych, dekoduje je
 * i zwraca wartość w milimetrach.
 *
 * @details
 * Przepływ operacji:
 *
 * 1. Wyzwolenie pomiaru:
 *    - TRIG_PIN → LOW (aktywuje suwmiarkę)
 *    - Suwmiarka zaczyna wysyłać dane po CLOCK_PIN
 *
 * 2. Oczekiwanie na dane:
 *    - Reset licznika bitów (bitCount = 0)
 *    - Reset flagi gotowości (dataReady = false)
 *    - Podłączenie ISR do CLOCK_PIN (zbocze opadające)
 *    - ISR (clockISR) odczytuje bity do bitBuffer
 *
 * 3. Timeout:
 *    - Maksymalny czas: MEASUREMENT_TIMEOUT_MS (200ms)
 *    - Jeśli dataReady nie ustawiona → timeout
 *    - Pętla sprawdza flagę co POLL_DELAY_MS (1ms)
 *
 * 4. Zakończenie pomiaru:
 *    - Odłączenie ISR od CLOCK_PIN
 *    - TRIG_PIN → HIGH (dezaktywuje suwmiarkę)
 *
 * 5. Dekodowanie (jeśli dataReady):
 *    - Odwrócenie kolejności bitów (reverseBits)
 *    - Dekodowanie BCD (decodeCaliper)
 *
 * 6. Walidacja wyniku:
 *    - Sprawdzenie zakresu: MEASUREMENT_MIN_VALUE do MEASUREMENT_MAX_VALUE
 *    - Sprawdzenie NaN (Not a Number)
 *    - Sprawdzenie Inf (Infinity)
 *    - Jeśli błąd → zwróć INVALID_MEASUREMENT_VALUE
 *
 * 7. Zwracanie wyniku:
 *    - Sukces: wartość w milimetrach
 *    - Błąd (timeout/invalid): INVALID_MEASUREMENT_VALUE
 *
 * Uwagi:
 * - Funkcja jest blokująca - czeka na timeout lub odebranie danych
 * - ISR działa w IRAM dla maksymalnej wydajności
 * - Timeout jest konieczny, aby zapobiec zawieszeniu przy braku danych
 *
 * @return Wartość pomiaru w milimetrach lub INVALID_MEASUREMENT_VALUE przy błędzie
 */
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
            DEBUG_I("Pomiar: %.3f mm", result);
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
