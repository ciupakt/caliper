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

        # GUI state: True when awaiting next measurement frame to apply calibration
        self.calibrate_pending: bool = False

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

        # NOTE: Calibrate is NOT disabled here. This path also runs on startup
        # when the 'g' refresh echoes a saved (non-empty) sessionName — that is
        # a state restore, not an explicit new-session action. Calibrate stays
        # active so it can be used immediately (only the manual "New Session"
        # button disables it, in MeasurementTab._confirm_new_session).

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
                        corrected_base = (
                            float(self.last_measurement_raw)
                            - float(self.current_calibration_offset)
                            + float(self.current_reference)
                        )
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
            # corrected = measurementRaw - calibrationOffset + reference
            # (matches firmware Master, main.cpp:702)
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
                        cal_corrected = raw - float(self.current_calibration_offset) + float(self.current_reference)
                        dpg.set_value("cal_corrected_display", f"Corrected: {cal_corrected:.3f} mm")
                except Exception:
                    pass

                corrected = raw - float(self.current_calibration_offset) + float(self.current_reference)

                # Calibration button flow: show pre-calibration corrected value in the
                # toolbar label, fill Settings offset field with raw and send it as offset.
                if self.calibrate_pending:
                    self.calibrate_pending = False
                    offset_val = self.calibration_tab._clamp_float(float(raw), -999.999, 999.999)
                    try:
                        if dpg.does_item_exist("cal_offset_input"):
                            dpg.set_value("cal_offset_input", offset_val)
                        if dpg.does_item_exist("calibration_label"):
                            dpg.set_value("calibration_label", f"Calibration: {corrected:.3f} mm")
                    except Exception:
                        pass
                    if self.calibration_tab._safe_write(self.serial_handler, f"c {offset_val:.3f}"):
                        self.calibration_tab.add_app_log(
                            f"[CALIBRATE] Sent c {offset_val:.3f} (offset=raw); label set to corrected"
                        )
                    else:
                        self.calibration_tab.add_app_log(
                            "[CALIBRATE] Port not open; offset not sent"
                        )

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
                    names = CalibrationTab.MOTOR_STATE_NAMES
                    if 0 <= state_val < len(names):
                        state_name = names[state_val]
                    else:
                        state_name = f"UNKNOWN({state_val})"
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

    def _on_calibrate(self, sender=None, app_data=None, user_data=None):
        """Toolbar 'Calibration' button: request a fresh measurement and, on the
        next `measurement:` frame, display the pre-calibration corrected value
        in the toolbar label, fill Settings `cal_offset_input` with the raw
        value and send it as offset (command `c`), identical to "Apply Offset".
        """
        # Immediate visible feedback in the Measurements toolbar label.
        try:
            port_open = (
                self.serial_handler is not None and self.serial_handler.is_open()
            )
        except Exception:
            port_open = False

        try:
            if dpg.does_item_exist("calibration_label"):
                if not port_open:
                    dpg.set_value("calibration_label", "Calibration: port closed!")
                    self.calibration_tab.add_app_log("[CALIBRATE] Port not open")
                    return
                dpg.set_value("calibration_label", "Calibration: measuring...")
        except Exception:
            pass

        if self.calibration_tab._safe_write(self.serial_handler, "m"):
            self.calibrate_pending = True
            self.calibration_tab._set_status("Sent: m (calibrate)")
            self.calibration_tab.add_app_log(
                "[CALIBRATE] Sent m, awaiting measurement to apply offset"
            )

    def _handle_reference_changed(self, new_ref):
        """Invoked when the user edits the Reference field in the Measurements toolbar.

        Enables the Calibrate button and synchronizes the new reference to the
        device (UART `v`), so the calibration targets the edited value and the
        field is not overwritten by the next `reference:` echo.
        """
        self.measurement_tab.set_calibrate_enabled(True)
        try:
            ref_val = self.calibration_tab._clamp_float(float(new_ref), -999.999, 999.999)
        except (TypeError, ValueError):
            return
        if self.serial_handler is not None and self.serial_handler.is_open():
            self.serial_handler.write(f"v {ref_val:.3f}")
            self.calibration_tab.add_app_log(f"[REFERENCE] Sent v {ref_val:.3f}")


    def key_press_handler(self, sender, key):
        """Handle keyboard shortcuts (key press events)"""
        # Hotkey: F1 = toggle Settings tab visibility
        if key == dpg.mvKey_F1:
            try:
                if dpg.does_item_exist("settings_tab"):
                    current = dpg.get_item_configuration("settings_tab").get("show", True)
                    dpg.configure_item("settings_tab", show=not current)
                    self.calibration_tab.add_app_log(
                        f"[HOTKEY] F1 -> Settings tab {'shown' if not current else 'hidden'}"
                    )
                return
            except Exception:
                pass

    def key_release_handler(self, sender, key):
        """Handle keyboard shortcuts (key release events)"""
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
            dpg.add_bool_value(tag="cal_autofill_next", default_value=False)
        
        # Font registry
        # DearPyGui 2.x automatically loads extended character ranges (incl. Polish
        # diacritics), so explicit add_font_range_hint/add_font_range calls are
        # no longer needed (and are deprecated no-ops).
        with dpg.font_registry():
            # OS detection for font paths
            if os.name == 'nt':  # Windows
                font_path = "C:/Windows/Fonts/segoeui.ttf"
                font_bold_path = "C:/Windows/Fonts/segoeuib.ttf"
            else:  # Linux/Unix
                font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
                font_bold_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

            with dpg.font(font_path, 22) as default_font:
                pass

            # Bold font (for emphasizing buttons, e.g. "Measure")
            with dpg.font(font_bold_path, 24, tag="font_bold"):
                pass

            # Small font (for logs – half the default size)
            with dpg.font(font_path, 13, tag="font_small"):
                pass

            # Log font (larger than font_small for better readability)
            with dpg.font(font_path, 16, tag="font_log"):
                pass

            # 2x font (twice the default 22) for Measurements toolbar controls
            with dpg.font(font_path, 44, tag="font_x2"):
                pass

            # 2x bold font for the "Calibration" label
            with dpg.font(font_bold_path, 44, tag="font_x2_bold"):
                pass

            # Gauge meta font (smaller – for timestamp/angle in Gauge tab)
            with dpg.font(font_path, 20, tag="font_gauge_meta"):
                pass

            # Gauge font (large – for displaying last measurement in Gauge tab)
            with dpg.font(font_bold_path, 360, tag="font_gauge"):
                pass
        
        # Handler registry
        with dpg.handler_registry():
            dpg.add_key_press_handler(callback=self.key_press_handler)
            dpg.add_key_release_handler(callback=self.key_release_handler)
        
        # Create viewport
        # Larger height so that chart and history are visible without clipping on startup.
        dpg.create_viewport(title="TKK DBMS 1.0", width=1200, height=850)

        # Main window
        with dpg.window(label="Caliper - Application", tag="main_window"):
            # Note: `dpg.tab` MUST have `mvTabBar` as parent.
            # We don't rely on `dpg.last_container()` (it can point to the last created container,
            # not the current `tab_bar`), instead we explicitly pass the identifier/tab tag.
            with dpg.tab_bar(tag="main_tab_bar") as tab_bar_id:
                # Measurements
                self.measurement_tab.create(
                    tab_bar_id,
                    self.serial_handler,
                    self.csv_handler,
                    on_drop=self._on_drop_measurement,
                    on_calibrate=self._on_calibrate,
                    on_reference_changed=self._handle_reference_changed,
                )

                # Gauge
                self.gauge_tab.create(tab_bar_id)

                # Calibration
                self.calibration_tab.create(tab_bar_id, self.serial_handler, self.csv_handler)

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
        try:
            self.measurement_tab.align_toolbar()
        except Exception:
            pass
    
    def _on_autoconnect_status(self, payload):
        """Callback for auto-connect status updates.

        Args:
            payload: tuple (display_text, connected_port|None)
        """
        display, port = payload
        # Connection status is shown in the window title bar.
        if port:
            status = f"Connected to {port}"
        elif display and display != "(none)":
            status = display
        else:
            status = None
        try:
            self.measurement_tab.set_connection_status(status)
        except Exception:
            pass
        try:
            self.calibration_tab.add_app_log(f"[AUTOCONNECT] {display}")
        except Exception:
            pass
        if port:
            try:
                if dpg.does_item_exist("port_combo"):
                    items = dpg.get_item_configuration("port_combo").get("items", [])
                    if port not in items:
                        items = self.serial_handler.list_ports()
                        dpg.configure_item("port_combo", items=items)
                    dpg.set_value("port_combo", port)
            except Exception:
                pass

    def _run_autoconnect(self):
        """Run auto-connect in background thread."""
        try:
            self.serial_handler.auto_connect(status_callback=self._on_autoconnect_status)
        except Exception as e:
            try:
                self.calibration_tab.add_app_log(f"[AUTOCONNECT] Error: {e}")
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

        # Start auto-connect after viewport is shown so status is visible
        self._autoconnect_thread = threading.Thread(
            target=self._run_autoconnect, daemon=True, name="autoconnect"
        )
        self._autoconnect_thread.start()

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
                if dpg.does_item_exist("meas_toolbar_row") and dpg.is_item_visible("meas_toolbar_row"):
                    self.measurement_tab.align_toolbar()
            except Exception:
                pass
            dpg.render_dearpygui_frame()

        dpg.destroy_context()
        
        # Cleanup
        self.serial_handler.stop_auto_connect()
        self.serial_handler.stop_reading()
        self.csv_handler.close()


def main():
    """Main entry point"""
    gui = CaliperGUI()
    gui.run()


if __name__ == "__main__":
    main()
