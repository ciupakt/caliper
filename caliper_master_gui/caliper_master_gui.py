"""
Caliper Master GUI - Main Application Entry Point
Refactored modular version
"""

import dearpygui.dearpygui as dpg
import threading
import time
import os
from datetime import datetime

# Import application modules
from src.serial_handler import SerialHandler
from src.utils.csv_handler import CSVHandler
from src.gui.measurement_tab import MeasurementTab
from src.gui.gauge_tab import GaugeTab
from src.gui.calibration_tab import CalibrationTab


class CaliperGUI:
    """Main GUI application class"""
    
    def __init__(self):
        self.serial_handler = SerialHandler()
        self.csv_handler = CSVHandler()
        self.measurement_tab = MeasurementTab()
        self.gauge_tab = GaugeTab()
        self.calibration_tab = CalibrationTab()

        # GUI state: last known offset (received from firmware via DEBUG_PLOT)
        self.current_calibration_offset: float = 0.0

        # GUI state: last known reference (received from firmware via DEBUG_PLOT)
        self.current_reference: float = 0.0

        # GUI state: last raw measurement (to calculate/refresh corrected in Calibration tab)
        self.last_measurement_raw: float | None = None
        
        # GUI state: last read angle X (from accelerometer)
        self.last_angle: str = ""
        
        # GUI state: current session name
        self.current_session_name: str = ""
        
        # GUI state: last saved session name (for detecting changes)
        self.last_saved_session_name: str = ""
    
    def _create_new_session_from_serial(self, session_name: str):
        """Create a new measurement session with CSV file from serial data.
        
        Args:
            session_name: Name of the session (will be used as CSV prefix)
        """
        # Close old CSV file if it exists
        if self.csv_handler.is_open():
            self.csv_handler.close()
        
        # Use session name as CSV file prefix
        filename = None
        try:
            filename = self.csv_handler.create_new_file(
                prefix=session_name,
                include_timestamp=self.measurement_tab.include_timestamp,
                include_angle=self.measurement_tab.include_angle,
                calibration_offset=self.current_calibration_offset,
                reference=self.current_reference,
            )
        except Exception as e:
            self.calibration_tab.add_app_log(f"ERROR: Failed to create CSV file: {str(e)}")
            return
        
        # Update last_saved_session_name
        self.last_saved_session_name = session_name
        
        # Clear measurement history in GUI
        self.measurement_tab._clear()
        self.gauge_tab.clear()
        
        # Update UI with new session info
        try:
            if filename:
                import os as _os
                self.measurement_tab._set_csv_info_label(_os.path.basename(filename))
        except Exception:
            pass
        
        # Update session_name in measurement_tab
        self.measurement_tab.session_name = session_name
        self.measurement_tab.csv_prefix = session_name
        
        # Log new session creation
        self.calibration_tab.add_app_log(f"[SESSION] New session created: {session_name} -> {filename}")

    @staticmethod
    def _normalize_debug_plot_line(data: str) -> str:
        """Normalize `DEBUG_PLOT` output by stripping leading '>' and whitespace."""
        if not data:
            return ""
        data = data.strip()
        return data[1:].strip() if data.startswith(">") else data

    def process_measurement_data(self, data: str):
        """Process measurement/plot data with validation and storage.

        We parse ONLY the frames that actually come from `DEBUG_PLOT` in
        [`caliper_master/src/main.cpp`](caliper_master/src/main.cpp:65).

        NOTE: `DEBUG_PLOT` always prepends '>' (see [`DEBUG_PLOT`](lib/CaliperShared/MacroDebugger.h:113)),
        so here we receive the already normalized line (without leading '>').
        """
        try:
            # --- Calibration (sent via DEBUG_PLOT on offset change and on measurement)
            if data.startswith("calibrationOffset:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    self.current_calibration_offset = float(val_str)
                except Exception:
                    # if it cannot be parsed, log only the text
                    self.calibration_tab.add_app_log(f"[CALIBRATION] Offset (parse err): {val_str}")
                    return

                self.calibration_tab.add_app_log(f"[CALIBRATION] Offset: {self.current_calibration_offset:.3f} mm")

                self.measurement_tab.calibration_offset = self.current_calibration_offset
                self.measurement_tab._show_measurements()

                # Refresh calibration UI (if it exists)
                try:
                    if dpg.does_item_exist("cal_offset_display"):
                        dpg.set_value("cal_offset_display", f"Current offset: {self.current_calibration_offset:.3f} mm")

                    if self.last_measurement_raw is not None and dpg.does_item_exist("cal_corrected_display"):
                        corrected_base = float(self.last_measurement_raw) + float(self.current_calibration_offset) if float(self.last_measurement_raw) < 0 else float(self.last_measurement_raw) - float(self.current_calibration_offset)
                        dpg.set_value("cal_corrected_display", f"Corrected: {corrected_base:.3f} mm")
                except Exception:
                    pass

                return

            # --- Reference (sent via DEBUG_PLOT on reference change and on settings refresh)
            if data.startswith("reference:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    self.current_reference = float(val_str)
                except Exception:
                    self.calibration_tab.add_app_log(f"[REFERENCE] Reference (parse err): {val_str}")
                    return

                self.calibration_tab.add_app_log(f"[REFERENCE] Reference: {self.current_reference:.3f} mm")

                self.measurement_tab.reference = self.current_reference
                self.measurement_tab._show_measurements()

                try:
                    if dpg.does_item_exist("ref_display"):
                        dpg.set_value("ref_display", f"Current reference: {self.current_reference:.3f} mm")
                    if dpg.does_item_exist("ref_input_meas"):
                        dpg.set_value("ref_input_meas", self.current_reference)
                except Exception:
                    pass

                return

            # --- Measurement (sent via DEBUG_PLOT in OnDataRecv)
            # Firmware Master sends raw measurement as `measurement:`.
            # GUI calculates correction on its side:
            # corrected = measurementRaw + current_calibration_offset
            if data.startswith("measurement:"):
                val_str = data.split(":", 1)[1].strip()
                raw = float(val_str)

                self.last_measurement_raw = float(raw)

                # Calibration: autofill offset field only on demand (button "Get raw value")
                try:
                    if dpg.does_item_exist("cal_autofill_next") and dpg.get_value("cal_autofill_next") is True:
                        if dpg.does_item_exist("cal_offset_input"):
                            dpg.set_value("cal_offset_input", float(raw))
                        dpg.set_value("cal_autofill_next", False)
                except Exception:
                    pass

                try:
                    if dpg.does_item_exist("cal_raw_display"):
                        dpg.set_value("cal_raw_display", f"Raw: {raw:.3f} mm")
                    if dpg.does_item_exist("cal_offset_display"):
                        dpg.set_value("cal_offset_display", f"Current offset: {self.current_calibration_offset:.3f} mm")
                    if dpg.does_item_exist("cal_corrected_display"):
                        cal_corrected = raw + float(self.current_calibration_offset) if raw < 0 else raw - float(self.current_calibration_offset)
                        dpg.set_value("cal_corrected_display", f"Corrected: {cal_corrected:.3f} mm")
                except Exception:
                    pass

                corrected = raw + float(self.current_calibration_offset) + float(self.current_reference)

                if -1000.0 <= corrected <= 1000.0:
                    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                    measurement_str = f"{corrected:.3f}"
                    self.measurement_tab.add_measurement(ts, measurement_str, float(corrected), self.last_angle)
                    self.gauge_tab.update(
                        measurement_str,
                        timestamp=ts,
                        angle=self.last_angle,
                        include_timestamp=self.measurement_tab.include_timestamp,
                        include_angle=self.measurement_tab.include_angle,
                    )

                    if self.csv_handler.is_open():
                        self.csv_handler.write_measurement(
                            self.measurement_tab.measurement_count,
                            measurement_str,
                            angle=self.last_angle,
                            timestamp=ts,
                        )
                else:
                    self.calibration_tab.add_app_log(f"ERROR: Value out of range (corrected): {corrected}")
                return

            if data.startswith("angleZ:"):
                angle_str = data.split(":", 1)[1].strip()
                self.last_angle = angle_str
                self.calibration_tab.add_app_log(f"[ANGLE Z] {angle_str}°")
                return

            if data.startswith("batteryVoltage:"):
                voltage_str = data.split(":", 1)[1].strip()
                self.calibration_tab.add_app_log(f"[BATTERY] {voltage_str} V")
                return

            # --- Measurement configuration (sent via DEBUG_PLOT on change of o/q/s/r)
            if data.startswith("timeout:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    timeout_val = int(val_str)
                    self.calibration_tab.add_app_log(f"[CONFIG] timeout: {timeout_val} ms")
                    # Refresh calibration UI (if it exists)
                    try:
                        if dpg.does_item_exist("tx_timeout_input"):
                            dpg.set_value("tx_timeout_input", timeout_val)
                    except Exception:
                        pass
                except Exception:
                    self.calibration_tab.add_app_log(f"[CONFIG] timeout (parse err): {val_str}")
                return

            if data.startswith("motorTorque:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    torque_val = int(val_str)
                    self.calibration_tab.add_app_log(f"[CONFIG] motorTorque: {torque_val}")
                    # Refresh calibration UI (if it exists)
                    try:
                        if dpg.does_item_exist("tx_torque_input"):
                            dpg.set_value("tx_torque_input", torque_val)
                    except Exception:
                        pass
                except Exception:
                    self.calibration_tab.add_app_log(f"[CONFIG] motorTorque (parse err): {val_str}")
                return

            if data.startswith("motorSpeed:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    speed_val = int(val_str)
                    self.calibration_tab.add_app_log(f"[CONFIG] motorSpeed: {speed_val}")
                    # Refresh calibration UI (if it exists)
                    try:
                        if dpg.does_item_exist("tx_speed_input"):
                            dpg.set_value("tx_speed_input", speed_val)
                    except Exception:
                        pass
                except Exception:
                    self.calibration_tab.add_app_log(f"[CONFIG] motorSpeed (parse err): {val_str}")
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
                    self.calibration_tab.add_app_log(f"[CONFIG] motorState: {state_name}")
                    
                    # Refresh calibration UI (if it exists)
                    try:
                        if dpg.does_item_exist("tx_state_input"):
                            dpg.set_value("tx_state_input", state_name)
                    except Exception:
                        pass
                except Exception:
                    self.calibration_tab.add_app_log(f"[CONFIG] motorState (parse err): {val_str}")
                return

            # --- Session name (sent via DEBUG_PLOT on session name change)
            if data.startswith("sessionName:"):
                name_str = data.split(":", 1)[1].strip()
                self.current_session_name = name_str
                
                # Check if name is non-empty and different from last saved
                if name_str and name_str != self.last_saved_session_name:
                    self._create_new_session_from_serial(name_str)
                else:
                    self.calibration_tab.add_app_log(f"[SESSION] Session name: {name_str}")
                return

            # --- DROP_MEAS (z RC przez Master)
            if data.startswith("dropMeas:"):
                val_str = data.split(":", 1)[1].strip()
                if val_str == "1":
                    if self.measurement_tab.drop_last_measurement():
                        self.csv_handler.remove_last_row()
                        self.calibration_tab.add_app_log("[RC] DROP_MEAS: last measurement removed")
                        self._sync_gauge()
                    else:
                        self.calibration_tab.add_app_log("[RC] DROP_MEAS: no measurements to remove")
                        self.gauge_tab.clear()
                return

            # --- Pairing status
            if data.startswith("pairing:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    if dpg.does_item_exist("pairing_status"):
                        if val_str == "1":
                            dpg.set_value("pairing_status", "Pairing: ACTIVE 10s")
                            self.calibration_tab.add_app_log("[PAIRING] Pairing mode active")
                        else:
                            dpg.set_value("pairing_status", "Pairing: inactive")
                            self.calibration_tab.add_app_log("[PAIRING] Pairing mode ended")
                except Exception:
                    pass
                return

            # --- Pairing countdown
            if data.startswith("pairingCountdown:"):
                val_str = data.split(":", 1)[1].strip()
                try:
                    if dpg.does_item_exist("pairing_status"):
                        dpg.set_value("pairing_status", f"Pairing: ACTIVE {val_str}s")
                except Exception:
                    pass
                return

            # Other (non-plot) lines are left as log (e.g. MOTOR)
            if "MOTOR" in data.upper() or "motor error" in data.lower():
                self.calibration_tab.add_app_log(f"[MOTOR] {data}")

        except ValueError as val_err:
            self.calibration_tab.add_app_log(f"ERROR: Invalid value - {str(val_err)}")
        except Exception as e:
            self.calibration_tab.add_app_log(f"ERROR processing data: {str(e)}")
    
    def serial_write_callback(self, data: str):
        """Callback for written serial data."""
        self.calibration_tab.add_serial_log(f"> {data}")

    def serial_data_callback(self, data: str):
        """Callback for received serial data."""
        self.calibration_tab.add_serial_log(f"< {data}")

        payload = self._normalize_debug_plot_line(data)

        # Process plot/measurement data (from DEBUG_PLOT)
        # (only keys actually emitted by firmware Master)
        if payload.startswith(
            (
                "measurement:",
                "angleZ:",
                "batteryVoltage:",
                "calibrationOffset:",
                "reference:",
                "timeout:",
                "motorTorque:",
                "motorSpeed:",
                "motorState:",
                "sessionName:",
                "dropMeas:",
                "pairing:",
                "pairingCountdown:",
            )
        ):
            self.process_measurement_data(payload)
            return

        # Motor / other status lines
        if "MOTOR" in payload.upper() or "motor error" in payload.lower():
            self.calibration_tab.add_app_log(f"[MOTOR] {payload}")
    
    def _sync_gauge(self, sender=None, app_data=None):
        """Sync gauge tab with current measurement_tab checkbox state and last measurement."""
        if not self.measurement_tab.meas_history:
            return
        last_ts, last_val, last_ang = self.measurement_tab.meas_history[-1]
        self.gauge_tab.update(
            last_val,
            timestamp=last_ts,
            angle=last_ang,
            include_timestamp=self.measurement_tab.include_timestamp,
            include_angle=self.measurement_tab.include_angle,
        )

    def _on_drop_measurement(self):
        """Callback after canceling last measurement from 'Cancel last measurement' button."""
        if self.measurement_tab.meas_history:
            self._sync_gauge()
        else:
            self.gauge_tab.clear()

    def key_press_handler(self, sender, key):
        """Handle keyboard shortcuts"""
        # Hotkey: 'm' = execute measurement (like clicking "Measure (m)")
        if key == dpg.mvKey_M:
            # Don't intercept if user is typing in a text field (e.g. session name)
            # or when any widget is active / session modal popup is open.
            try:
                if dpg.is_any_item_active() or dpg.is_any_item_focused():
                    return
                if dpg.does_item_exist("new_session_popup") and dpg.is_item_shown("new_session_popup"):
                    return
                if dpg.does_item_exist("session_name_input") and dpg.is_item_focused("session_name_input"):
                    return
            except Exception:
                pass

            if self.serial_handler is None or not self.serial_handler.is_open():
                return

            self.serial_handler.write("m")
            self.calibration_tab.add_app_log("[HOTKEY] m -> measure")
            return
    
    def create_gui(self):
        """Create the main GUI"""
        dpg.create_context()

        # Value registry (flags/states used by callbacks)
        with dpg.value_registry():
            # One-time auto-fill of offset after clicking "Get raw value"
            dpg.add_bool_value(tag="cal_autofill_next", default_value=False)
        
        # Font registry
        # DearPyGui may not have the Latin Extended character range loaded by default,
        # so we explicitly add the ranges needed for Polish characters.
        with dpg.font_registry():
            # OS detection for font paths
            if os.name == 'nt':  # Windows
                font_path = "C:/Windows/Fonts/segoeui.ttf"
                font_bold_path = "C:/Windows/Fonts/segoeuib.ttf"
            else:  # Linux/Unix
                font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
                font_bold_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

            with dpg.font(font_path, 22) as default_font:
                dpg.add_font_range_hint(dpg.mvFontRangeHint_Default)
                # Latin Extended-A (e.g. Polish diacritics)
                dpg.add_font_range(0x0100, 0x017F)
                # Latin Extended-B (just in case)
                dpg.add_font_range(0x0180, 0x024F)

            # Bold font (for emphasizing buttons, e.g. "Measure")
            with dpg.font(font_bold_path, 24, tag="font_bold"):
                dpg.add_font_range_hint(dpg.mvFontRangeHint_Default)
                dpg.add_font_range(0x0100, 0x017F)
                dpg.add_font_range(0x0180, 0x024F)

            # Small font (for logs – half the default size)
            with dpg.font(font_path, 13, tag="font_small"):
                dpg.add_font_range_hint(dpg.mvFontRangeHint_Default)
                dpg.add_font_range(0x0100, 0x017F)
                dpg.add_font_range(0x0180, 0x024F)

            # Gauge meta font (smaller – for timestamp/angle in Gauge tab)
            with dpg.font(font_path, 20, tag="font_gauge_meta"):
                dpg.add_font_range_hint(dpg.mvFontRangeHint_Default)
                dpg.add_font_range(0x0100, 0x017F)
                dpg.add_font_range(0x0180, 0x024F)

            # Gauge font (large – for displaying last measurement in Gauge tab)
            with dpg.font(font_bold_path, 360, tag="font_gauge"):
                dpg.add_font_range_hint(dpg.mvFontRangeHint_Default)
                dpg.add_font_range(0x0100, 0x017F)
                dpg.add_font_range(0x0180, 0x024F)
        
        # Handler registry
        with dpg.handler_registry():
            dpg.add_key_release_handler(callback=self.key_press_handler)
        
        # Create viewport
        # Larger height so that chart and history are visible without clipping on startup.
        dpg.create_viewport(title="TKK Caliper 1.0", width=1200, height=850)

        # Main window
        with dpg.window(label="Caliper - Application", tag="main_window"):
            # Note: `dpg.tab` MUST have `mvTabBar` as parent.
            # We don't rely on `dpg.last_container()` (it can point to the last created container,
            # not the current `tab_bar`), instead we explicitly pass the identifier/tab tag.
            with dpg.tab_bar(tag="main_tab_bar") as tab_bar_id:
                # Measurements
                self.measurement_tab.create(tab_bar_id, self.serial_handler, self.csv_handler, on_drop=self._on_drop_measurement)

                # Gauge
                self.gauge_tab.create(tab_bar_id)

                # Calibration
                self.calibration_tab.create(tab_bar_id, self.serial_handler)

        # Bind font
        dpg.bind_font(default_font)

        # Main window fills the entire viewport and scales with it.
        dpg.set_primary_window("main_window", True)

        # Re-center measurement in Gauge tab on viewport resize.
        dpg.set_viewport_resize_callback(self._on_viewport_resize)

        # Sync gauge tab when timestamp/angle checkboxes change
        with dpg.item_handler_registry(tag="gauge_sync_handler"):
            dpg.add_item_active_handler(callback=self._sync_gauge)
        if dpg.does_item_exist("timestamp_cb"):
            dpg.bind_item_handler_registry("timestamp_cb", "gauge_sync_handler")
        if dpg.does_item_exist("angle_cb"):
            dpg.bind_item_handler_registry("angle_cb", "gauge_sync_handler")

    def _on_viewport_resize(self, sender=None, app_data=None):
        """Recalculate centering of measurement in Gauge tab after viewport resize."""
        try:
            self.gauge_tab.recenter()
        except Exception:
            pass
        # Align status row in Measurements tab (Session: ↔ Connected to:)
        try:
            self.measurement_tab.update_status_row_layout()
        except Exception:
            pass
    
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

        # Manual render loop: allows continuous re-centering of measurement
        # in Gauge tab (child_window size known only after rendering
        # and changes on window scaling and tab switching).
        while dpg.is_dearpygui_running():
            try:
                if dpg.does_item_exist("gauge_root") and dpg.is_item_visible("gauge_value"):
                    self.gauge_tab.recenter()
            except Exception:
                pass
            try:
                # Live update of status row alignment (Session ↔ Connected to)
                if dpg.does_item_exist("status_row") and dpg.is_item_visible("status_row"):
                    self.measurement_tab.update_status_row_layout()
            except Exception:
                pass
            dpg.render_dearpygui_frame()

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
