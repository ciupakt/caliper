"""
Calibration Tab GUI Component for Caliper Master GUI

Zawiera:
- Konfigurację parametrów pomiaru (msgMaster.* wysyłane po UART)
- Kalibrację lokalną Mastera (localCalibrationOffset)

UWAGA: tagi kontrolek są celowo takie same jak wcześniej w `MeasurementTab`, bo:
- `CaliperGUI.process_measurement_data()` odświeża `cal_offset_input` oraz pola podglądu (surowy/offset/skorygowany)
"""

import dearpygui.dearpygui as dpg


class CalibrationTab:
    """Calibration tab component"""

    def create(self, parent: int, serial_handler):
        """Create the calibration tab UI"""
        with dpg.tab(label="Kalibracja", parent=parent):
            dpg.add_text("Kalibracja i konfiguracja pomiaru", color=(100, 200, 255))
            dpg.add_spacer(height=5)

            # Status tylko dla tej zakładki (żeby feedback był widoczny nawet gdy user nie jest w 'Pomiary')
            dpg.add_text("", tag="cal_tab_status")
            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                # --- Konfiguracja pomiaru
                with dpg.group():
                    dpg.add_text("Konfiguracja pomiaru:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)

                    dpg.add_input_int(
                        label="msgMaster.timeout (ms)",
                        tag="tx_timeout_input",
                        default_value=0,
                        min_value=0,
                        max_value=600000,
                        width=220,
                    )
                    dpg.add_input_int(
                        label="msgMaster.motorTorque (0-255)",
                        tag="tx_torque_input",
                        default_value=0,
                        min_value=0,
                        max_value=255,
                        width=220,
                    )
                    dpg.add_input_int(
                        label="msgMaster.motorSpeed (0-255)",
                        tag="tx_speed_input",
                        default_value=255,
                        min_value=0,
                        max_value=255,
                        width=220,
                    )

                    dpg.add_spacer(height=5)
                    dpg.add_button(
                        label="Zastosuj",
                        callback=self._apply_measurement_config,
                        width=220,
                        height=30,
                        user_data=serial_handler,
                    )
                    dpg.add_spacer(height=5)
                    dpg.add_text("Komendy UART: o <ms>, q <0-255>, s <0-255>", color=(150, 150, 150))

                dpg.add_spacer(width=30)

                # --- Kalibracja
                with dpg.group():
                    dpg.add_text("Kalibracja (Master lokalnie):", color=(100, 200, 255))
                    dpg.add_spacer(height=5)

                    # Podgląd jak w WWW: surowy / aktualny offset / skorygowany
                    dpg.add_text("Surowy: n/a", tag="cal_raw_display")
                    dpg.add_text("Aktualny offset: 0.000 mm", tag="cal_offset_display", color=(198, 40, 40))
                    dpg.add_text("Skorygowany: n/a", tag="cal_corrected_display")
                    dpg.add_spacer(height=5)

                    dpg.add_input_float(
                        label="localCalibrationOffset (mm)",
                        tag="cal_offset_input",
                        default_value=0.0,
                        min_value=-14.999,
                        max_value=14.999,
                        format="%.3f",
                        width=180,
                    )

                    dpg.add_button(
                        label="Pobierz bieżący pomiar",
                        callback=self._calibration_measure,
                        width=220,
                        height=30,
                        user_data=serial_handler,
                    )
                    dpg.add_button(
                        label="Zastosuj offset",
                        callback=self._apply_calibration_offset,
                        width=220,
                        height=30,
                        user_data=serial_handler,
                    )

                    dpg.add_spacer(height=10)
                    dpg.add_text("ESP32 Master WWW: http://192.168.4.1", color=(100, 255, 100))
                    # Wartości zgodne z [`config.h`](caliper_master/src/config.h:33)
                    dpg.add_text("WiFi: Orange_WiFi (hasło: 1670$2026)", color=(100, 255, 100))

    @staticmethod
    def _clamp_int(val: int, vmin: int, vmax: int) -> int:
        return max(vmin, min(vmax, int(val)))

    @staticmethod
    def _clamp_float(val: float, vmin: float, vmax: float) -> float:
        return max(vmin, min(vmax, float(val)))

    def _set_status(self, text: str):
        if dpg.does_item_exist("cal_tab_status"):
            dpg.set_value("cal_tab_status", text)

        # Jeśli istnieje też globalny status (np. w zakładce 'Pomiary'), aktualizujemy go równolegle
        if dpg.does_item_exist("status"):
            dpg.set_value("status", text)

    def _safe_write(self, serial_handler, data: str) -> bool:
        """Write to serial only when port is open; update status otherwise."""
        if serial_handler is None or not hasattr(serial_handler, "is_open"):
            self._set_status("BŁĄD: Brak SerialHandler")
            return False

        if not serial_handler.is_open():
            self._set_status("BŁĄD: Port nie jest otwarty")
            return False

        serial_handler.write(data)
        return True

    def _calibration_measure(self, sender, app_data, user_data):
        """Pobierz bieżący pomiar (jak w WWW).

        Realizowane przez wysłanie komendy 'm'. Po nadejściu ramki `measurement:`
        w [`CaliperGUI.process_measurement_data()`](caliper_master_gui/caliper_master_gui.py:44)
        zostanie jednorazowo uzupełnione pole `cal_offset_input`.
        """
        serial_handler = user_data

        try:
            if dpg.does_item_exist("cal_autofill_next"):
                dpg.set_value("cal_autofill_next", True)
        except Exception:
            pass

        if self._safe_write(serial_handler, "m"):
            self._set_status("Wysłano: m (pobierz bieżący pomiar)")

    def _apply_calibration_offset(self, sender, app_data, user_data):
        """Zastosuj offset (jak w WWW) – UART: c <±14.999>."""
        serial_handler = user_data
        try:
            val = float(dpg.get_value("cal_offset_input"))
        except Exception:
            self._set_status("BŁĄD: Nieprawidłowy localCalibrationOffset")
            return

        val = self._clamp_float(val, -14.999, 14.999)
        dpg.set_value("cal_offset_input", val)

        if self._safe_write(serial_handler, f"c {val:.3f}"):
            self._set_status(f"Wysłano: c {val:.3f} (zastosuj offset)")

    def _apply_measurement_config(self, sender, app_data, user_data):
        """Apply msgMaster config fields on Master via UART commands: o/q/s."""
        serial_handler = user_data

        try:
            timeout_ms = int(dpg.get_value("tx_timeout_input"))
            torque = int(dpg.get_value("tx_torque_input"))
            speed = int(dpg.get_value("tx_speed_input"))
        except Exception:
            self._set_status("BŁĄD: Nieprawidłowe wartości konfiguracji")
            return

        timeout_ms = self._clamp_int(timeout_ms, 0, 600000)
        torque = self._clamp_int(torque, 0, 255)
        speed = self._clamp_int(speed, 0, 255)

        dpg.set_value("tx_timeout_input", timeout_ms)
        dpg.set_value("tx_torque_input", torque)
        dpg.set_value("tx_speed_input", speed)

        if not self._safe_write(serial_handler, f"o {timeout_ms}"):
            return
        self._safe_write(serial_handler, f"q {torque}")
        self._safe_write(serial_handler, f"s {speed}")

        self._set_status(f"Wysłano: o {timeout_ms}, q {torque}, s {speed}")
