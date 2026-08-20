/**
 * @file rs485.h
 * @brief RS485 (MAX485) sensor interface for ESP32
 * @author System Generated
 * @date 2026-08-14
 * @version 1.0
 *
 * @details
 * Provides interface for reading measurements from a Sylvac S_Probe P12D
 * (or compatible RS485 instrument) over a MAX485 transceiver in ASCII mode.
 *
 * Communication parameters (per doc/Quickstart-guide-S_Probe-P12D-Open_WEB.pdf):
 * - ASCII mode: 115200 Bd, 8 data bits, no parity, one stop bit (8N1)
 * - All commands terminated by a carriage return (CR, 0x0D)
 * - Position query command: '?' ("Get the probe's position")
 *
 * The MAX485 is a half-duplex transceiver: the DE/RE control pin selects
 * between transmit (HIGH) and receive (LOW) mode. The DE and RE signals
 * are driven together from a single GPIO (RS485_DE_RE_PIN).
 *
 * This class exposes the same public API as CaliperInterface so that
 * main.cpp can select between SPC and RS485 via conditional compilation.
 *
 * Built only when the RS485 build flag is defined.
 *
 * @version 1.0 - Initial implementation for RS485 ASCII interface
 */

#ifndef RS485_H
#define RS485_H

#if defined(RS485)

#include <Arduino.h>
#include "../config.h"
#include <shared_common.h>
#include <error_handler.h>

/**
 * @brief RS485 (MAX485) sensor interface driver
 *
 * Half-duplex ASCII driver for a Sylvac S_Probe P12D probe connected through
 * a MAX485 transceiver. Mirrors the public API of CaliperInterface so the
 * sensor backend can be swapped at compile time via build flags.
 */
class RS485Interface {
private:
    /**
     * @brief Switch the MAX485 into transmit mode
     * @details Drives DE/RE HIGH, enabling the driver and disabling the receiver
     *          so that transmitted data does not echo back into the RX line.
     */
    void setTransmitMode();

    /**
     * @brief Switch the MAX485 into receive mode
     * @details Drives DE/RE LOW, disabling the driver and enabling the receiver
     *          so that the probe response can be read.
     */
    void setReceiveMode();

    /**
     * @brief Send the position query command ('?' + CR) over RS485
     * @details Switches to transmit mode, writes the query, waits for the TX
     *          FIFO to drain, then switches back to receive mode.
     * @return true if the query was sent successfully
     */
    bool sendQuery();

    /**
     * @brief Read the ASCII response line from the probe
     * @details Reads characters from Serial1 until a CR/LF terminator or
     *          timeout (RS485_RESPONSE_TIMEOUT_MS). Parses the numeric value
     *          with strtof.
     * @return Measurement value in millimeters, or INVALID_MEASUREMENT_VALUE
     *         on timeout / empty / unparseable response.
     */
    float readResponse();

public:
    /**
     * @brief Initialize the RS485 interface
     * @details Configures the DE/RE control pin (default receive mode) and
     *          starts Serial1 at RS485_BAUD_RATE, 8N1, on the RS485 RX/TX pins.
     */
    void begin();

    /**
     * @brief Perform a measurement
     * @return Measured value in millimeters, or INVALID_MEASUREMENT_VALUE on error
     * @details Sends the '?' query, reads the response, and validates the result.
     *
     * Possible errors:
     * - ERR_RS485_TIMEOUT: No response from the probe within RS485_RESPONSE_TIMEOUT_MS
     * - ERR_RS485_INVALID_RESPONSE: Response empty or could not be parsed
     * - ERR_RS485_OUT_OF_RANGE: Measurement value out of valid range
     */
    float performMeasurement();

    /**
     * @brief Perform a reliable measurement
     * @return Measured value in millimeters, or INVALID_MEASUREMENT_VALUE on error
     * @details Basic implementation delegates to performMeasurement() once and
     *          returns its result. Kept for API parity with CaliperInterface
     *          (main.cpp calls this method). May be enhanced later with a
     *          two-identical-readings filter like the SPC implementation.
     */
    float performReliableMeasurement();
};

#endif // defined(RS485)

#endif // RS485_H
