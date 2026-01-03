"""
Caliper Master GUI - Main Application Entry Point
Refactored modular version
"""

import dearpygui.dearpygui as dpg
import threading
import time
from datetime import datetime

# Import application modules
from src.serial_handler import SerialHandler
from src.utils.csv_handler import CSVHandler
from src.gui.measurement_tab import MeasurementTab
from src.gui.calibration_tab import CalibrationTab


class CaliperGUI:
    """Main GUI application class"""
    
    def __init__(self):
        self.serial_handler = SerialHandler()
        self.csv_handler = CSVHandler()
        self.measurement_tab = MeasurementTab()
        self.calibration_tab = CalibrationTab()

        # Stan GUI: ostatni znany offset (przychodzi z firmware przez DEBUG_PLOT)
        self.current_calibration_offset: float = 0.0

        # Stan GUI: ostatni surowy pomiar (żeby móc policzyć/odświeżyć skorygowany w zakładce Kalibracja)
        self.last_measurement_raw: float | None = None
    
    @staticmethod
    def _normalize_debug_plot_line(data: str) -> str:
        """Normalize `DEBUG_PLOT` output by stripping leading '>' and whitespace."""
        if not data:
            return ""
        data = data.strip()
        return data[1:].strip() if data.startswith(">") else data

    def process_measurement_data(self, data: str):
        """Process measurement/plot data with validation and storage.

        Parsujemy WYŁĄCZNIE te ramki, które faktycznie wychodzą z `DEBUG_PLOT` w
        [`caliper_master/src/main.cpp`](caliper_master/src/main.cpp:65).

        NOTE: `DEBUG_PLOT` zawsze prepends '>' (see [`DEBUG_PLOT`](lib/CaliperShared/MacroDebugger.h:113)),
        więc tu dostajemy już linię znormalizowaną (bez wiodącego '>').
        """
        try:
            # --- Kalibracja (wysyłane przez DEBUG_PLOT przy zmianie offsetu i przy pomiarze)
            if data.startswith("calibrationOffset:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    self.current_calibration_offset = float(val_str)
                except Exception:
                    # jeśli nie da się sparsować, logujemy tylko tekst
                    self.calibration_tab.add_app_log(f"[KALIBRACJA] Offset (parse err): {val_str}")
                    return

                self.calibration_tab.add_app_log(f"[KALIBRACJA] Offset: {self.current_calibration_offset:.3f} mm")

                # Odświeżamy UI kalibracji (jeśli istnieje)
                try:
                    if dpg.does_item_exist("cal_offset_display"):
                        dpg.set_value("cal_offset_display", f"Aktualny offset: {self.current_calibration_offset:.3f} mm")

                    # Jeśli mamy ostatni surowy pomiar, odświeżamy też skorygowany
                    if self.last_measurement_raw is not None and dpg.does_item_exist("cal_corrected_display"):
                        corrected = float(self.last_measurement_raw) + float(self.current_calibration_offset)
                        dpg.set_value("cal_corrected_display", f"Skorygowany: {corrected:.3f} mm")
                except Exception:
                    pass

                return

            # --- Pomiar (wysyłane przez DEBUG_PLOT w OnDataRecv)
            # Firmware Master wysyła surowy pomiar jako `measurement:`.
            # GUI liczy korekcję po swojej stronie:
            # corrected = measurementRaw + current_calibration_offset
            if data.startswith("measurement:"):
                val_str = data.split(":", 1)[1].strip()
                raw = float(val_str)

                self.last_measurement_raw = float(raw)

                # Kalibracja: autofill pola offsetu tylko na żądanie (przycisk "Pobierz bieżący pomiar")
                try:
                    if dpg.does_item_exist("cal_autofill_next") and dpg.get_value("cal_autofill_next") is True:
                        if dpg.does_item_exist("cal_offset_input"):
                            # clamp jak w firmware/UI (-14.999..14.999)
                            raw_clamped = max(-14.999, min(14.999, float(raw)))
                            dpg.set_value("cal_offset_input", raw_clamped)
                        dpg.set_value("cal_autofill_next", False)
                except Exception:
                    pass

                corrected = raw + float(self.current_calibration_offset)

                # Odświeżamy UI kalibracji (jeśli istnieje)
                try:
                    if dpg.does_item_exist("cal_raw_display"):
                        dpg.set_value("cal_raw_display", f"Surowy: {raw:.3f} mm")
                    if dpg.does_item_exist("cal_offset_display"):
                        dpg.set_value("cal_offset_display", f"Aktualny offset: {self.current_calibration_offset:.3f} mm")
                    if dpg.does_item_exist("cal_corrected_display"):
                        dpg.set_value("cal_corrected_display", f"Skorygowany: {corrected:.3f} mm")
                except Exception:
                    pass

                # Validate range (na wykresie/logach trzymamy skorygowaną wartość)
                if -1000.0 <= corrected <= 1000.0:
                    ts = datetime.now().isoformat(timespec="seconds")
                    measurement_str = f"{corrected:.3f} mm"
                    self.measurement_tab.add_measurement(ts, measurement_str, float(corrected))

                    if self.csv_handler.is_open():
                        self.csv_handler.write_measurement(measurement_str, ts if self.measurement_tab.include_timestamp else None)
                else:
                    self.calibration_tab.add_app_log(f"BLAD: Wartosc poza zakresem (corrected): {corrected}")
                return

            if data.startswith("angleX:"):
                angle_str = data.split(":", 1)[1].strip()
                self.calibration_tab.add_app_log(f"[ANGLE X] {angle_str}°")
                return

            if data.startswith("batteryVoltage:"):
                voltage_str = data.split(":", 1)[1].strip()
                self.calibration_tab.add_app_log(f"[BATERIA] {voltage_str} V")
                return

            # --- Konfiguracja pomiaru (wysyłane przez DEBUG_PLOT przy zmianie o/q/s/r)
            if data.startswith("timeout:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    timeout_val = int(val_str)
                    self.calibration_tab.add_app_log(f"[KONFIG] timeout: {timeout_val} ms")
                    # Odświeżamy UI kalibracji (jeśli istnieje)
                    try:
                        if dpg.does_item_exist("tx_timeout_input"):
                            dpg.set_value("tx_timeout_input", timeout_val)
                    except Exception:
                        pass
                except Exception:
                    self.calibration_tab.add_app_log(f"[KONFIG] timeout (parse err): {val_str}")
                return

            if data.startswith("motorTorque:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    torque_val = int(val_str)
                    self.calibration_tab.add_app_log(f"[KONFIG] motorTorque: {torque_val}")
                    # Odświeżamy UI kalibracji (jeśli istnieje)
                    try:
                        if dpg.does_item_exist("tx_torque_input"):
                            dpg.set_value("tx_torque_input", torque_val)
                    except Exception:
                        pass
                except Exception:
                    self.calibration_tab.add_app_log(f"[KONFIG] motorTorque (parse err): {val_str}")
                return

            if data.startswith("motorSpeed:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    speed_val = int(val_str)
                    self.calibration_tab.add_app_log(f"[KONFIG] motorSpeed: {speed_val}")
                    # Odświeżamy UI kalibracji (jeśli istnieje)
                    try:
                        if dpg.does_item_exist("tx_speed_input"):
                            dpg.set_value("tx_speed_input", speed_val)
                    except Exception:
                        pass
                except Exception:
                    self.calibration_tab.add_app_log(f"[KONFIG] motorSpeed (parse err): {val_str}")
                return

            if data.startswith("motorState:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    state_val = int(val_str)
                    state_names = {
                        0: "MOTOR_STOP (0)",
                        1: "MOTOR_FORWARD (1)",
                        2: "MOTOR_REVERSE (2)",
                        3: "MOTOR_BRAKE (3)"
                    }
                    state_name = state_names.get(state_val, f"UNKNOWN({state_val})")
                    self.calibration_tab.add_app_log(f"[KONFIG] motorState: {state_name}")
                    
                    # Odświeżamy UI kalibracji (jeśli istnieje)
                    try:
                        if dpg.does_item_exist("tx_state_input"):
                            dpg.set_value("tx_state_input", state_name)
                    except Exception:
                        pass
                except Exception:
                    self.calibration_tab.add_app_log(f"[KONFIG] motorState (parse err): {val_str}")
                return

            # Inne (nie-plot) linie zostawiamy jako log (np. SILNIK)
            if "SILNIK" in data.upper() or "blad silnika" in data.lower():
                self.calibration_tab.add_app_log(f"[SILNIK] {data}")

        except ValueError as val_err:
            self.calibration_tab.add_app_log(f"BLAD: Nieprawidlowa wartosc - {str(val_err)}")
        except Exception as e:
            self.calibration_tab.add_app_log(f"BLAD przetwarzania danych: {str(e)}")
    
    def serial_write_callback(self, data: str):
        """Callback for written serial data."""
        self.calibration_tab.add_serial_log(f"> {data}")

    def serial_data_callback(self, data: str):
        """Callback for received serial data."""
        self.calibration_tab.add_serial_log(f"< {data}")

        payload = self._normalize_debug_plot_line(data)

        # Process plot/measurement data (from DEBUG_PLOT)
        # (tylko klucze faktycznie emitowane przez firmware Master)
        if payload.startswith(
            (
                "measurement:",
                "angleX:",
                "batteryVoltage:",
                "calibrationOffset:",
                "timeout:",
                "motorTorque:",
                "motorSpeed:",
                "motorState:",
            )
        ):
            self.process_measurement_data(payload)
            return

        # Motor / other status lines
        if "SILNIK" in payload.upper() or "blad silnika" in payload.lower():
            self.calibration_tab.add_app_log(f"[SILNIK] {payload}")
    
    def key_press_handler(self, sender, key):
        """Handle keyboard shortcuts"""
        # Hotkey: 'p' = wykonaj pomiar (jak kliknięcie "Wykonaj pomiar")
        if key == dpg.mvKey_P:
            # Jeśli user aktualnie pisze w polu tekstowym, nie przechwytujemy.
            try:
                if dpg.is_any_item_active() or dpg.is_any_item_focused():
                    return
            except Exception:
                pass

            if self.serial_handler is None or not self.serial_handler.is_open():
                try:
                    if dpg.does_item_exist("status"):
                        dpg.set_value("status", "BŁĄD: Port nie jest otwarty (hotkey: p)")
                except Exception:
                    pass
                return

            self.serial_handler.write("m")
            try:
                if dpg.does_item_exist("status"):
                    dpg.set_value("status", "Hotkey: p → wykonaj pomiar")
            except Exception:
                pass
            self.calibration_tab.add_app_log("[HOTKEY] p -> m")
            return
    
    def create_gui(self):
        """Create the main GUI"""
        dpg.create_context()

        # Value registry (flagi/stany wykorzystywane przez callbacki)
        with dpg.value_registry():
            # Jednorazowe auto-uzupełnienie offsetu po kliknięciu "Pobierz bieżący pomiar"
            dpg.add_bool_value(tag="cal_autofill_next", default_value=False)
        
        # Font registry
        # DearPyGui domyślnie może nie mieć załadowanego zakresu znaków Latin Extended,
        # więc jawnie dodajemy zakresy potrzebne dla polskich znaków.
        with dpg.font_registry():
            with dpg.font("C:/Windows/Fonts/segoeui.ttf", 22) as default_font:
                dpg.add_font_range_hint(dpg.mvFontRangeHint_Default)
                # Latin Extended-A (m.in. ą, ć, ę, ł, ń, ó, ś, ź, ż)
                dpg.add_font_range(0x0100, 0x017F)
                # Latin Extended-B (na wszelki wypadek)
                dpg.add_font_range(0x0180, 0x024F)

            # Font pogrubiony (do akcentowania przycisków, np. "Wykonaj pomiar")
            with dpg.font("C:/Windows/Fonts/segoeuib.ttf", 24, tag="font_bold"):
                dpg.add_font_range_hint(dpg.mvFontRangeHint_Default)
                dpg.add_font_range(0x0100, 0x017F)
                dpg.add_font_range(0x0180, 0x024F)
        
        # Handler registry
        with dpg.handler_registry():
            dpg.add_key_release_handler(callback=self.key_press_handler)
        
        # Create viewport
        # Większa wysokość, żeby wykres i historia były widoczne bez ucinania po starcie.
        dpg.create_viewport(title="Caliper - aplikacja COM", width=1200, height=850)

        # Main window
        with dpg.window(label="Caliper - aplikacja", width=1180, height=810):
            # Uwaga: `dpg.tab` MUSI mieć jako rodzica `mvTabBar`.
            # Nie polegamy na `dpg.last_container()` (potrafi wskazać ostatnio utworzony kontener,
            # a nie aktualny `tab_bar`), tylko przekazujemy jawnie identyfikator/tab tag.
            with dpg.tab_bar(tag="main_tab_bar") as tab_bar_id:
                # Pomiary
                self.measurement_tab.create(tab_bar_id, self.serial_handler, self.csv_handler)

                # Kalibracja
                self.calibration_tab.create(tab_bar_id, self.serial_handler)

        # Bind font
        dpg.bind_font(default_font)
    
    def run(self):
        """Run the application"""
        # Set up serial data callbacks
        self.serial_handler.set_data_callback(self.serial_data_callback)
        self.serial_handler.set_write_callback(self.serial_write_callback)
        
        # Start serial reading
        self.serial_handler.start_reading()
        
        # Setup and show GUI
        self.create_gui()
        dpg.setup_dearpygui()
        dpg.show_viewport()
        dpg.start_dearpygui()
        dpg.destroy_context()
        
        # Cleanup
        self.serial_handler.stop_reading()
        self.csv_handler.close()


def main():
    """Main entry point"""
    gui = CaliperGUI()
    gui.run()


if __name__ == "__main__":
    main()
