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
        with dpg.tab(label="Kalibracja", parent=parent):
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
                            "MOTOR_BRAKE (3)"
                        ],
                        default_value="MOTOR_STOP (0)",
                        width=220,
                    )

                    dpg.add_spacer(height=5)
                    with dpg.group(horizontal=True):
                        dpg.add_button(
                            label="Zastosuj",
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
                            label="Odśwież ustawienia",
                            callback=self._refresh_settings,
                            width=150,
                            height=30,
                            user_data=serial_handler,
                        )
                    dpg.add_spacer(height=5)

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
                        label="calibrationOffset (mm)",
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

            dpg.add_spacer(height=10)
            dpg.add_separator()
            dpg.add_spacer(height=10)

            # --- Obszary logów
            dpg.add_separator()
            dpg.add_spacer(height=10)

            with dpg.group(horizontal=True):
                # Log komunikacji serial
                with dpg.group():
                    dpg.add_text("Log komunikacji serial:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    dpg.add_input_text(
                        multiline=True,
                        readonly=True,
                        width=560,
                        height=200,
                        tag="cal_serial_log",
                    )
                    # Dodaj handler kliknięcia do czyszczenia logów
                    with dpg.item_handler_registry():
                        dpg.add_item_clicked_handler(callback=self._on_log_clicked, user_data="serial")
                    dpg.bind_item_handler_registry("cal_serial_log", dpg.last_container())

                dpg.add_spacer(width=20)

                # Log aplikacji
                with dpg.group():
                    dpg.add_text("Log aplikacji:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    dpg.add_input_text(
                        multiline=True,
                        readonly=True,
                        width=560,
                        height=200,
                        tag="cal_app_log",
                    )
                    # Dodaj handler kliknięcia do czyszczenia logów
                    with dpg.item_handler_registry():
                        dpg.add_item_clicked_handler(callback=self._on_log_clicked, user_data="app")
                    dpg.bind_item_handler_registry("cal_app_log", dpg.last_container())

    def add_serial_log(self, line: str):
        """Add a line to the serial log with timestamp"""
        timestamp = datetime.now().strftime("[%H:%M:%S.%f]")[:-3]  # [HH:MM:SS.mmm]
        self.serial_log_lines.append(f"{timestamp} {line}")
        if dpg.does_item_exist("cal_serial_log"):
            dpg.set_value("cal_serial_log", "\n".join(list(self.serial_log_lines)))

    def add_app_log(self, line: str):
        """Add a line to the app log with timestamp"""
        timestamp = datetime.now().strftime("[%H:%M:%S.%f]")[:-3]  # [HH:MM:SS.mmm]
        self.app_log_lines.append(f"{timestamp} {line}")
        if dpg.does_item_exist("cal_app_log"):
            dpg.set_value("cal_app_log", "\n".join(list(self.app_log_lines)))

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
            self._set_status("BŁĄD: Nieprawidłowy calibrationOffset")
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
            
            # Parsowanie motorState z combo boxa (format: "MOTOR_STOP (0)")
            state_str = dpg.get_value("tx_state_input")
            state = int(state_str.split("(")[-1].rstrip(")"))
        except Exception:
            self._set_status("BŁĄD: Nieprawidłowe wartości konfiguracji")
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

        self._set_status(f"Wysłano: o {timeout_ms}, q {torque}, s {speed}")

    def _send_motortest(self, sender, app_data, user_data):
        """Send motor test command via UART: r <state> i t."""
        serial_handler = user_data
        
        try:
            # Parsowanie motorState z combo boxa (format: "MOTOR_STOP (0)")
            state_str = dpg.get_value("tx_state_input")
            state = int(state_str.split("(")[-1].rstrip(")"))
        except Exception:
            self._set_status("BŁĄD: Nieprawidłowa wartość motorState")
            return
        
        state = self._clamp_int(state, 0, 3)
        
        # Wysyłamy komendę r <state> przed testem silnika
        if self._safe_write(serial_handler, f"r {state}"):
            self._set_status(f"Wysłano: r {state}, t")
            self.add_app_log(f"[GUI] Wysłano: r {state}, t")
            self._safe_write(serial_handler, "t")

    def _refresh_settings(self, sender, app_data, user_data):
        """Refresh all settings from Master via UART command 'g'."""
        serial_handler = user_data
        
        if self._safe_write(serial_handler, "g"):
            self._set_status("Wysłano: g (odśwież ustawienia)")
            self.add_app_log("[GUI] Wysłano: g (odśwież ustawienia)")

    def _on_log_clicked(self, sender, app_data, user_data):
        """Handle double click on log areas - clear logs on double click"""
        current_time = time.time()
        
        # Sprawdź, czy to podwójne kliknięcie
        if current_time - self.last_click_time < self.double_click_threshold:
            # To podwójne kliknięcie - wyczyść odpowiednie logi
            log_type = user_data
            if log_type == "serial":
                self.serial_log_lines.clear()
                if dpg.does_item_exist("cal_serial_log"):
                    dpg.set_value("cal_serial_log", "")
            elif log_type == "app":
                self.app_log_lines.clear()
                if dpg.does_item_exist("cal_app_log"):
                    dpg.set_value("cal_app_log", "")
        
        # Zaktualizuj czas ostatniego kliknięcia
        self.last_click_time = current_time

    def clear_logs(self):
        """Clear all log lines"""
        self.serial_log_lines.clear()
        self.app_log_lines.clear()
        if dpg.does_item_exist("cal_serial_log"):
            dpg.set_value("cal_serial_log", "")
        if dpg.does_item_exist("cal_app_log"):
            dpg.set_value("cal_app_log", "")
