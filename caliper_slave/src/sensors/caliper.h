/**
 * @file caliper.h
 * @brief Caliper (suwmiarka) sensor interface for ESP32
 * @author System Generated
 * @date 2025-12-27
 * @version 2.0
 *
 * @details
 * Provides interface for reading measurements from digital caliper
 * using clock/data protocol with interrupt-based data capture.
 *
 * @version 2.0 - Integrated comprehensive error code system
 */

#ifndef CALIPER_H
#define CALIPER_H

#include <Arduino.h>
#include "../config.h"
#include <shared_common.h>
#include <error_handler.h>

class CaliperInterface {
private:
    static volatile uint8_t bitBuffer[52];
    static volatile int bitCount;
    static volatile bool dataReady;
    
    static void IRAM_ATTR clockISR();
    void reverseBits();
    float decodeCaliper();
    
public:
    /**
     * @brief Initialize caliper interface
     * @details Configures pins for clock, data, and trigger signals
     */
    void begin();
    
    /**
     * @brief Perform a measurement
     * @return Measured value in millimeters, or INVALID_MEASUREMENT_VALUE on error
     * @details Triggers measurement, captures data via interrupt, and decodes result
     *
     * Possible errors:
     * - ERR_CALIPER_TIMEOUT: Measurement timeout after MEASUREMENT_TIMEOUT_MS
     * - ERR_CALIPER_INVALID_DATA: Invalid data received from caliper
     * - ERR_CALIPER_OUT_OF_RANGE: Measurement value out of valid range
     */
    float performMeasurement();
    
    /**
     * @brief Check if measurement data is ready
     * @return true if data is ready to be read
     */
    bool isDataReady() const { return dataReady; }
};

#endif // CALIPER_H
