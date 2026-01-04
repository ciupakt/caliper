#ifndef MEASUREMENT_STATE_H
#define MEASUREMENT_STATE_H

#include <Arduino.h>

/**
 * @brief Klasa enkapsulująca stan pomiarowy systemu
 *
 * Klasa zarządza stanem pomiarowym, w tym ostatnimi wartościami pomiarów,
 * napięciem baterii oraz flagą gotowości pomiaru. Stan jest przechowywany
 * w buforach tekstowych o stałym rozmiarze dla bezpieczeństwa.
 *
 * Użycie:
 * ```cpp
 * static MeasurementState measurementState;
 *
 * // Ustawienie pomiaru
 * measurementState.setMeasurement(123.456f);
 *
 * // Pobranie flagi gotowości
 * if (measurementState.isReady()) {
 *     float value = measurementState.getValue();
 * }
 *
 * // Resetowanie flagi gotowości
 * measurementState.setReady(false);
 * ```
 */
class MeasurementState
{
private:
    static constexpr size_t MEASUREMENT_BUFFER_SIZE = 32;
    static constexpr size_t BATTERY_BUFFER_SIZE = 32;

    char lastMeasurement[MEASUREMENT_BUFFER_SIZE];
    char lastBatteryVoltage[BATTERY_BUFFER_SIZE];
    float lastValue;
    bool ready;
    bool measurementInProgress;

public:
    /**
     * @brief Konstruktor - inicjalizuje stan domyślny
     */
    MeasurementState();

    /**
     * @brief Ustawia wartość pomiaru i formatuje tekst
     *
     * @param value Wartość pomiaru w milimetrach
     */
    void setMeasurement(float value);

    /**
     * @brief Ustawia napięcie baterii i formatuje tekst
     *
     * @param voltage Napięcie w woltach
     */
    void setBatteryVoltage(float voltage);

    /**
     * @brief Ustawia tekst pomiaru (np. komunikat statusu)
     *
     * @param message Tekst do ustawienia
     */
    void setMeasurementMessage(const char *message);

    /**
     * @brief Ustawia flagę gotowości pomiaru
     *
     * @param isReady Stan gotowości (true/false)
     */
    void setReady(bool isReady);

    /**
     * @brief Pobiera tekst ostatniego pomiaru
     *
     * @return Wskaźnik do bufora z tekstem pomiaru
     */
    const char *getMeasurement() const;

    /**
     * @brief Pobiera tekst ostatniego napięcia baterii
     *
     * @return Wskaźnik do bufora z tekstem napięcia
     */
    const char *getBatteryVoltage() const;

    /**
     * @brief Pobiera wartość liczbową ostatniego pomiaru
     *
     * @return Wartość pomiaru w milimetrach
     */
    float getValue() const;

    /**
     * @brief Sprawdza czy pomiar jest gotowy
     *
     * @return true jeśli pomiar jest gotowy, false w przeciwnym razie
     */
    bool isReady() const;

    /**
     * @brief Sprawdza czy operacja pomiarowa jest w toku
     *
     * @return true jeśli operacja jest w toku, false w przeciwnym razie
     */
    bool isMeasurementInProgress() const;

    /**
     * @brief Ustawia flagę operacji pomiarowej w toku
     *
     * @param inProgress Stan operacji (true/false)
     */
    void setMeasurementInProgress(bool inProgress);

    /**
     * @brief Resetuje stan do wartości domyślnych
     */
    void reset();
};

#endif // MEASUREMENT_STATE_H
