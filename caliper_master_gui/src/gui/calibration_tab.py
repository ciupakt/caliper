"""
Calibration Tab GUI Component for Caliper Master GUI

Zawiera:
- Konfigurację parametrów pomiaru (msgMaster.* wysyłane po UART):
  - timeout (ms) - komenda UART: o <ms>
  - motorTorque (0-255) - komenda UART: q <0-255>
  - motorSpeed (0-255) - komenda UART: s <0-255>
  - motorState (0-3) - komenda UART: r <0-3> (combo box z enum MotorState)
  - Test silnika - komenda UART: r <state> + t (przycisk "Motortest")
- Kalibrację lokalną Mastera (calibrationOffset)
- Dwa obszary logów: komunikacja serial i logi aplikacji

Enum MotorState:
  - 0 = MOTOR_STOP (Motor stopped/coast)
  - 1 = MOTOR_FORWARD (Motor rotating forward)
  - 2 = MOTOR_REVERSE (Motor rotating reverse)
  - 3 = MOTOR_BRAKE (Motor braking)

UWAGA: tagi kontrolek są używane przez `CaliperGUI.process_measurement_data()` do
odświeżania pól podglądu (surowy/offset/skorygowany) oraz pól konfiguracyjnych
(timeout, motorTorque, motorSpeed, motorState).

KOMENDA 'r': jest wysyłana tylko po wciśnięciu przycisku "Motortest".
Przycisk "Zastosuj" wysyła tylko komendy o, q, s (bez r).
"""

import dearpygui.dearpygui as dpg
from collections import deque
import threading
import time
from datetime import datetime


class CalibrationTab:
    """Calibration tab component"""

    def __init__(self, max_lines: int = 200):
        self.max_lines = max_lines
        self.serial_log_lines = deque(maxlen=max_lines)
        self.app_log_lines = deque(maxlen=max_lines)
        # Śledzenie czasu ostatniego kliknięcia do wykrywania podwójnego kliknięcia
        self.last_click_time = 0.0
        self.double_click_threshold = 0.5  # sekundy

    def create(self, parent: int, serial_handler):
        """Create the calibration tab UI"""
        with dpg.tab(label="Calibration", parent=parent):
            with dpg.child_window(no_scrollbar=True, tag="cal_tab_root"):
                dpg.add_spacer(height=5)

                dpg.add_text("", tag="cal_tab_status")
                dpg.add_spacer(height=10)

                with dpg.group(horizontal=True):
                    # --- Konfiguracja pomiaru
                    with dpg.group():
                        dpg.add_text("Measurement Configuration:", color=(100, 200, 255))
                        dpg.add_spacer(height=5)

                        dpg.add_input_int(
                            label="timeout (ms)",
                            tag="tx_timeout_input",
                            default_value=0,
                            min_value=0,
                            max_value=600000,
                            width=220,
                        )
                        dpg.add_input_int(
                            label="motorTorque (0-255)",
                            tag="tx_torque_input",
                            default_value=0,
                            min_value=0,
                            max_value=255,
                            width=220,
                        )
                        dpg.add_input_int(
                            label="motorSpeed (0-255)",
                            tag="tx_speed_input",
                            default_value=255,
                            min_value=0,
                            max_value=255,
                            width=220,
                        )
                        dpg.add_combo(
                            label="motorState",
                            tag="tx_state_input",
                            items=[
                                "MOTOR_STOP (0)",
                                "MOTOR_FORWARD (1)",
                                "MOTOR_REVERSE (2)",
                                "MOTOR_BRAKE (3)",
                            ],
                            default_value="MOTOR_STOP (0)",
                            width=220,
                        )

                        dpg.add_spacer(height=5)
                        with dpg.group(horizontal=True):
                            dpg.add_button(
                                label="Apply",
                                callback=self._apply_measurement_config,
                                width=150,
                                height=30,
                                user_data=serial_handler,
                            )
                            dpg.add_button(
                                label="Motortest",
                                callback=self._send_motortest,
                                width=150,
                                height=30,
                                user_data=serial_handler,
                            )
                            dpg.add_button(
                                label="Refresh Settings",
                                callback=self._refresh_settings,
                                width=150,
                                height=30,
                                user_data=serial_handler,
                            )
                        with dpg.group(horizontal=True):
                            dpg.add_button(
                                label="OTA (Slave)",
                                callback=self._send_ota,
                                width=150,
                                height=30,
                                user_data=serial_handler,
                            )
                            dpg.add_button(
                                label="Pairing",
                                callback=self._send_pairing,
                                width=150,
                                height=30,
                                user_data=serial_handler,
                            )
                        dpg.add_spacer(height=5)
                        dpg.add_text("", tag="pairing_status")

                    dpg.add_spacer(width=30)

                    # --- Kalibracja
                    with dpg.group():
                        dpg.add_text("Calibration (Master local):", color=(100, 200, 255))
                        dpg.add_spacer(height=5)

                        dpg.add_text("Raw: n/a", tag="cal_raw_display")
                        dpg.add_text(
                            "Current offset: 0.000 mm",
                            tag="cal_offset_display",
                            color=(198, 40, 40),
                        )
                        dpg.add_text("Corrected: n/a", tag="cal_corrected_display")
                        dpg.add_spacer(height=5)

                        dpg.add_input_float(
                            label="calibrationOffset (mm)",
                            tag="cal_offset_input",
                            default_value=0.0,
                            min_value=-14.999,
                            max_value=14.999,
                            format="%.3f",
                            width=180,
                        )

                        dpg.add_button(
                            label="Get Current Measurement",
                            callback=self._calibration_measure,
                            width=220,
                            height=30,
                            user_data=serial_handler,
                        )
                        dpg.add_button(
                            label="Apply Offset",
                            callback=self._apply_calibration_offset,
                            width=220,
                            height=30,
                            user_data=serial_handler,
                        )

                        dpg.add_spacer(height=10)
                        dpg.add_text(
                            "ESP32 Master Web: http://192.168.4.1", color=(100, 255, 100)
                        )
                        dpg.add_text(
                            "WiFi: Orange_WiFi (password: 1670$2026)", color=(100, 255, 100)
                        )

                dpg.add_spacer(height=5)
                dpg.add_separator()
                dpg.add_spacer(height=5)

                with dpg.group(horizontal=True):
                    # Log komunikacji serial
                    with dpg.group():
                        dpg.add_text(
                            "Serial Communication Log (dblclick = clear):",
                            color=(100, 200, 255),
                        )
                        dpg.add_input_text(
                            multiline=True,
                            readonly=True,
                            width=555,
                            height=300,
                            tag="cal_serial_log",
                        )
                        if dpg.does_item_exist("font_small"):
                            dpg.bind_item_font("cal_serial_log", "font_small")
                        with dpg.item_handler_registry(tag="serial_log_click_handler"):
                            dpg.add_item_clicked_handler(
                                callback=self._on_log_clicked, user_data="serial"
                            )
                        dpg.bind_item_handler_registry(
                            "cal_serial_log", "serial_log_click_handler"
                        )

                    dpg.add_spacer(width=20)

                    # Log aplikacji
                    with dpg.group():
                        dpg.add_text(
                            "App Log (dblclick = clear):", color=(100, 200, 255)
                        )
                        dpg.add_input_text(
                            multiline=True,
                            readonly=True,
                            width=555,
                            height=300,
                            tag="cal_app_log",
                        )
                        if dpg.does_item_exist("font_small"):
                            dpg.bind_item_font("cal_app_log", "font_small")
                        with dpg.item_handler_registry(tag="app_log_click_handler"):
                            dpg.add_item_clicked_handler(
                                callback=self._on_log_clicked, user_data="app"
                            )
                        dpg.bind_item_handler_registry(
                            "cal_app_log", "app_log_click_handler"
                        )

    def add_serial_log(self, line: str):
        """Add a line to the serial log with timestamp"""
        timestamp = datetime.now().strftime("[%H:%M:%S.%f]")[:-3]  # [HH:MM:SS.mmm]
        self.serial_log_lines.append(f"{timestamp} {line}")
        while len(self.serial_log_lines) > self.max_lines:
            self.serial_log_lines.popleft()
        if dpg.does_item_exist("cal_serial_log"):
            dpg.set_value("cal_serial_log", "\n".join(self.serial_log_lines))

            def _autoscroll():
                try:
                    if dpg.does_item_exist("cal_serial_log"):
                        dpg.set_y_scroll(
                            "cal_serial_log", dpg.get_y_scroll_max("cal_serial_log")
                        )
                except Exception:
                    pass

            threading.Timer(0.05, _autoscroll).start()

    def add_app_log(self, line: str):
        """Add a line to the app log with timestamp"""
        timestamp = datetime.now().strftime("[%H:%M:%S.%f]")[:-3]  # [HH:MM:SS.mmm]
        self.app_log_lines.append(f"{timestamp} {line}")
        while len(self.app_log_lines) > self.max_lines:
            self.app_log_lines.popleft()
        if dpg.does_item_exist("cal_app_log"):
            dpg.set_value("cal_app_log", "\n".join(self.app_log_lines))

            def _autoscroll():
                try:
                    if dpg.does_item_exist("cal_app_log"):
                        dpg.set_y_scroll(
                            "cal_app_log", dpg.get_y_scroll_max("cal_app_log")
                        )
                except Exception:
                    pass

            threading.Timer(0.05, _autoscroll).start()

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
            self._set_status("ERROR: No SerialHandler")
            return False

        if not serial_handler.is_open():
            self._set_status("ERROR: Port not open")
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
            self._set_status("Sent: m (get current measurement)")

    def _apply_calibration_offset(self, sender, app_data, user_data):
        """Zastosuj offset (jak w WWW) – UART: c <±14.999>."""
        serial_handler = user_data
        try:
            val = float(dpg.get_value("cal_offset_input"))
        except Exception:
            self._set_status("ERROR: Invalid calibrationOffset")
            return

        val = self._clamp_float(val, -14.999, 14.999)
        dpg.set_value("cal_offset_input", val)

        if self._safe_write(serial_handler, f"c {val:.3f}"):
            self._set_status(f"Sent: c {val:.3f} (apply offset)")

    def _apply_measurement_config(self, sender, app_data, user_data):
        """Apply msgMaster config fields on Master via UART commands: o/q/s."""
        serial_handler = user_data

        try:
            timeout_ms = int(dpg.get_value("tx_timeout_input"))
            torque = int(dpg.get_value("tx_torque_input"))
            speed = int(dpg.get_value("tx_speed_input"))

            # Parsowanie motorState z combo boxa (format: "MOTOR_STOP (0)")
            state_str = dpg.get_value("tx_state_input")
            state = int(state_str.split("(")[-1].rstrip(")"))
        except Exception:
            self._set_status("ERROR: Invalid configuration values")
            return

        timeout_ms = self._clamp_int(timeout_ms, 0, 600000)
        torque = self._clamp_int(torque, 0, 255)
        speed = self._clamp_int(speed, 0, 255)
        state = self._clamp_int(state, 0, 3)

        dpg.set_value("tx_timeout_input", timeout_ms)
        dpg.set_value("tx_torque_input", torque)
        dpg.set_value("tx_speed_input", speed)
        dpg.set_value("tx_state_input", state)

        if not self._safe_write(serial_handler, f"o {timeout_ms}"):
            return
        self._safe_write(serial_handler, f"q {torque}")
        self._safe_write(serial_handler, f"s {speed}")

        self._set_status(f"Sent: o {timeout_ms}, q {torque}, s {speed}")

    def _send_motortest(self, sender, app_data, user_data):
        """Send motor test command via UART: r <state> i t."""
        serial_handler = user_data

        try:
            # Parsowanie motorState z combo boxa (format: "MOTOR_STOP (0)")
            state_str = dpg.get_value("tx_state_input")
            state = int(state_str.split("(")[-1].rstrip(")"))
        except Exception:
            self._set_status("ERROR: Invalid motorState value")
            return

        state = self._clamp_int(state, 0, 3)

        # Wysyłamy komendę r <state> przed testem silnika
        if self._safe_write(serial_handler, f"r {state}"):
            self._set_status(f"Sent: r {state}, t")
            self.add_app_log(f"[GUI] Sent: r {state}, t")
            self._safe_write(serial_handler, "t")

    def _refresh_settings(self, sender, app_data, user_data):
        """Refresh all settings from Master via UART command 'g'."""
        serial_handler = user_data

        if self._safe_write(serial_handler, "g"):
            self._set_status("Sent: g (refresh settings)")
            self.add_app_log("[GUI] Sent: g (refresh settings)")

    def _send_ota(self, sender, app_data, user_data):
        """Send OTA command to Slave via UART command 'f'."""
        serial_handler = user_data

        if self._safe_write(serial_handler, "f"):
            self._set_status("Sent: f (OTA update)")
            self.add_app_log("[GUI] Sent: f (OTA update)")

    def _send_pairing(self, sender, app_data, user_data):
        """Send pairing command via UART command 'p'."""
        serial_handler = user_data

        if self._safe_write(serial_handler, "p"):
            self._set_status("Sent: p (pairing mode)")
            self.add_app_log("[GUI] Sent: p (pairing mode)")

    def _on_log_clicked(self, sender, app_data, user_data):
        """Handle double click on log areas - clear logs on double click"""
        current_time = time.time()

        if current_time - self.last_click_time < self.double_click_threshold:
            log_type = user_data
            if log_type == "serial":
                self.serial_log_lines.clear()
                if dpg.does_item_exist("cal_serial_log"):
                    dpg.set_value("cal_serial_log", "")
            elif log_type == "app":
                self.app_log_lines.clear()
                if dpg.does_item_exist("cal_app_log"):
                    dpg.set_value("cal_app_log", "")

        self.last_click_time = current_time

    def clear_logs(self):
        """Clear all log lines"""
        self.serial_log_lines.clear()
        self.app_log_lines.clear()
        if dpg.does_item_exist("cal_serial_log"):
            dpg.set_value("cal_serial_log", "")
        if dpg.does_item_exist("cal_app_log"):
            dpg.set_value("cal_app_log", "")
