#ifndef MEASUREMENT_STATE_H
#define MEASUREMENT_STATE_H

#include <Arduino.h>

/**
 * @brief Class encapsulating the measurement state of the system
 *
 * The class manages the measurement state, including recent measurement values,
 * battery voltage, and the measurement ready flag. State is stored
 * in fixed-size text buffers for safety.
 *
 * Usage:
 * ```cpp
 * static MeasurementState measurementState;
 *
 * // Set measurement
 * measurementState.setMeasurement(123.456f);
 *
 * // Get ready flag
 * if (measurementState.isReady()) {
 *     float value = measurementState.getValue();
 * }
 *
 * // Reset ready flag
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
     * @brief Constructor - initializes default state
     */
    MeasurementState();

    /**
     * @brief Sets measurement value and formats text
     *
     * @param value Measurement value in millimeters
     */
    void setMeasurement(float value);

    /**
     * @brief Sets battery voltage and formats text
     *
     * @param voltage Voltage in volts
     */
    void setBatteryVoltage(float voltage);

    /**
     * @brief Sets measurement text (e.g. status message)
     *
     * @param message Text to set
     */
    void setMeasurementMessage(const char *message);

    /**
     * @brief Sets the measurement ready flag
     *
     * @param isReady Ready state (true/false)
     */
    void setReady(bool isReady);

    /**
     * @brief Gets the text of the last measurement
     *
     * @return Pointer to measurement text buffer
     */
    const char *getMeasurement() const;

    /**
     * @brief Gets the last battery voltage text
     *
     * @return Pointer to voltage text buffer
     */
    const char *getBatteryVoltage() const;

    /**
     * @brief Gets the numeric value of the last measurement
     *
     * @return Measurement value in millimeters
     */
    float getValue() const;

    /**
     * @brief Checks if measurement is ready
     *
     * @return true if measurement is ready, false otherwise
     */
    bool isReady() const;

    /**
     * @brief Checks if a measurement operation is in progress
     *
     * @return true if operation is in progress, false otherwise
     */
    bool isMeasurementInProgress() const;

    /**
     * @brief Sets the measurement operation in progress flag
     *
     * @param inProgress Operation state (true/false)
     */
    void setMeasurementInProgress(bool inProgress);

    /**
     * @brief Resets state to default values
     */
    void reset();
};

#endif // MEASUREMENT_STATE_H
