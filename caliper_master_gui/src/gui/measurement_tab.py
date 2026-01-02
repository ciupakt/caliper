"""
Measurement Tab GUI Component for Caliper Master GUI
"""

import dearpygui.dearpygui as dpg
from collections import deque
from datetime import datetime


class MeasurementTab:
    """Measurement tab component for displaying measurements and controls"""
    
    def __init__(self, max_history: int = 1000, max_plot_points: int = 500):
        self.max_history = max_history
        self.max_plot_points = max_plot_points
        self.meas_history = deque(maxlen=max_history)
        self.plot_x = deque(maxlen=max_plot_points)
        self.plot_y = deque(maxlen=max_plot_points)
        self.measurement_count = 0
        self.include_timestamp = False
    
    def create(self, parent: int, serial_handler, csv_handler):
        """Create the measurement tab UI"""
        with dpg.tab(label="Measurement", parent=parent):
            with dpg.group(horizontal=True):
                # Port Configuration Group
                with dpg.group():
                    dpg.add_text("COM Port Configuration:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    ports_list = serial_handler.list_ports()
                    dpg.add_combo(ports_list, tag="port_combo", width=250)
                    if ports_list:
                        dpg.set_value("port_combo", ports_list[0])
                    dpg.add_spacer(height=5)
                    dpg.add_button(label="Refresh ports", callback=self._refresh_ports, width=150, height=30, user_data=serial_handler)
                    dpg.add_button(label="Open port", callback=self._open_port, width=150, height=30, user_data=(serial_handler, csv_handler))
                    dpg.add_spacer(height=5)
                    dpg.add_text("Status: Not connected", tag="status")
                    dpg.add_text("", tag="csv_info")

                    dpg.add_spacer(height=10)
                    dpg.add_text("Measurement Configuration:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    dpg.add_input_int(
                        label="msgMaster.timeout (ms)",
                        tag="tx_timeout_input",
                        default_value=0,
                        min_value=0,
                        max_value=600000,
                        width=200,
                    )
                    dpg.add_input_int(
                        label="msgMaster.motorTorque (0-255)",
                        tag="tx_torque_input",
                        default_value=0,
                        min_value=0,
                        max_value=255,
                        width=200,
                    )
                    dpg.add_input_int(
                        label="msgMaster.motorSpeed (0-255)",
                        tag="tx_speed_input",
                        default_value=255,
                        min_value=0,
                        max_value=255,
                        width=200,
                    )
                    dpg.add_spacer(height=5)
                    dpg.add_button(
                        label="Zastosuj",
                        callback=self._apply_measurement_config,
                        width=200,
                        height=30,
                        user_data=serial_handler,
                    )
                    dpg.add_spacer(height=5)
                    dpg.add_text("UART cmds: o <ms>, q <0-255>, s <0-255>", color=(150, 150, 150))

                dpg.add_spacer(width=30)

                # Measurement Controls Group
                with dpg.group():
                    dpg.add_text("Measurement Controls:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    dpg.add_button(label="Trigger measurement", callback=self._trigger, width=200, height=30, user_data=serial_handler)
                    dpg.add_button(label="Clear measurements", callback=self._clear, width=200, height=30, user_data=csv_handler)
                    dpg.add_spacer(height=5)
                    dpg.add_checkbox(label="Auto trigger", tag="auto_checkbox", callback=self._set_auto, user_data=serial_handler)
                    dpg.add_input_int(label="Interval (ms)", tag="interval_ms", default_value=1000, min_value=500, enabled=False, width=150)
                    dpg.add_spacer(height=5)
                    dpg.add_checkbox(label="Include timestamp", callback=self._timestamp_checkbox, tag="timestamp_cb")

                    dpg.add_spacer(height=10)
                    dpg.add_text("Calibration (Master local):", color=(100, 200, 255))
                    dpg.add_input_float(
                        label="localCalibrationOffset (mm)",
                        tag="cal_offset_input",
                        default_value=0.0,
                        min_value=-14.999,
                        max_value=14.999,
                        format="%.3f",
                        width=150,
                    )
                    dpg.add_button(
                        label="Zastosuj",
                        callback=self._apply_calibration_offset,
                        width=200,
                        height=30,
                        user_data=serial_handler,
                    )

                    dpg.add_spacer(height=10)
                    dpg.add_text("ESP32 Master: http://192.168.4.1", color=(100, 255, 100))
                    dpg.add_text("Connect WiFi: ESP32_Pomiar", color=(100, 255, 100))

            dpg.add_separator()
            dpg.add_spacer(height=10)
            
            # Measurement History
            dpg.add_text("Measurement History:")
            with dpg.child_window(width=840, height=120, tag="meas_scroll"):
                dpg.add_group(tag="meas_container")
            
            dpg.add_spacer(height=10)
            
            # Live Plot
            dpg.add_text("Live Plot:")
            with dpg.plot(label="Measurements", height=250, width=840):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Measurement #", tag="x_axis")
                dpg.add_plot_axis(dpg.mvYAxis, label="Value", tag="y_axis")
                dpg.add_line_series([], [], label="Measurement", parent="y_axis", tag="plot_data")
    
    def _refresh_ports(self, sender, app_data, user_data):
        """Refresh the list of available ports"""
        serial_handler = user_data
        ports = serial_handler.list_ports()
        dpg.configure_item("port_combo", items=ports)
        if ports:
            dpg.set_value("port_combo", ports[0])
    
    def _open_port(self, sender, app_data, user_data):
        """Open the selected serial port"""
        serial_handler, csv_handler = user_data
        port = dpg.get_value("port_combo")
        
        if serial_handler.open_port(port):
            dpg.set_value("status", f"Connected to {port}")
            filename = csv_handler.create_new_file(self.include_timestamp)
            dpg.set_value("csv_info", f"CSV file: {filename}")
        else:
            dpg.set_value("status", "ERR: Failed to open port")
    
    def _trigger(self, sender, app_data, user_data):
        """Send trigger command"""
        serial_handler = user_data
        serial_handler.write("m")
    
    def _clear(self, sender, app_data, user_data):
        """Clear all measurements"""
        csv_handler = user_data
        self.meas_history.clear()
        self.plot_x.clear()
        self.plot_y.clear()
        self.measurement_count = 0
        dpg.set_value("plot_data", [list(self.plot_x), list(self.plot_y)])
        self._show_measurements()
        
        # Create new CSV file
        filename = csv_handler.create_new_file(self.include_timestamp)
        dpg.set_value("csv_info", f"CSV file: {filename}")
        dpg.set_value("status", "Measurements cleared, new CSV created")
    
    def _set_auto(self, sender, app_data, user_data):
        """Toggle auto trigger"""
        serial_handler = user_data
        running = dpg.get_value("auto_checkbox")
        dpg.configure_item("interval_ms", enabled=running)
        # Note: Auto trigger thread implementation would go here
    
    def _timestamp_checkbox(self, sender, app_data, user_data):
        """Toggle timestamp inclusion"""
        self.include_timestamp = app_data
        self._show_measurements()
    
    @staticmethod
    def _clamp_int(val: int, vmin: int, vmax: int) -> int:
        return max(vmin, min(vmax, int(val)))

    @staticmethod
    def _clamp_float(val: float, vmin: float, vmax: float) -> float:
        return max(vmin, min(vmax, float(val)))

    def _safe_write(self, serial_handler, data: str) -> bool:
        """Write to serial only when port is open; update status otherwise."""
        if serial_handler is None or not hasattr(serial_handler, "is_open"):
            dpg.set_value("status", "ERR: Serial handler missing")
            return False

        if not serial_handler.is_open():
            dpg.set_value("status", "ERR: Port not open")
            return False

        serial_handler.write(data)
        return True

    def _apply_calibration_offset(self, sender, app_data, user_data):
        """Apply localCalibrationOffset on Master via UART command: c <±14.999>."""
        serial_handler = user_data
        try:
            val = float(dpg.get_value("cal_offset_input"))
        except Exception:
            dpg.set_value("status", "ERR: Invalid localCalibrationOffset")
            return

        val = self._clamp_float(val, -14.999, 14.999)
        dpg.set_value("cal_offset_input", val)
        if self._safe_write(serial_handler, f"c {val:.3f}"):
            dpg.set_value("status", f"Sent: c {val:.3f}")

    def _apply_measurement_config(self, sender, app_data, user_data):
        """Apply msgMaster config fields on Master via UART commands: o/q/s."""
        serial_handler = user_data

        try:
            timeout_ms = int(dpg.get_value("tx_timeout_input"))
            torque = int(dpg.get_value("tx_torque_input"))
            speed = int(dpg.get_value("tx_speed_input"))
        except Exception:
            dpg.set_value("status", "ERR: Invalid config values")
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
        dpg.set_value("status", f"Sent: o {timeout_ms}, q {torque}, s {speed}")
    
    def add_measurement(self, timestamp: str, value: str, numeric_value: float):
        """Add a measurement to history and plot"""
        self.meas_history.append((timestamp, value))
        self.measurement_count += 1
        
        # Update plot
        self.plot_x.append(self.measurement_count)
        self.plot_y.append(numeric_value)
        
        # Update GUI
        dpg.set_value("plot_data", [list(self.plot_x), list(self.plot_y)])
        self._update_plot_axes()
        self._show_measurements()
    
    def _show_measurements(self):
        """Display measurements in the history view"""
        if dpg.does_item_exist("meas_container"):
            dpg.delete_item("meas_container", children_only=True)
        
        # Show only last 50 measurements for performance
        recent_measurements = list(self.meas_history)[-50:]
        start_idx = max(1, len(self.meas_history) - len(recent_measurements) + 1)
        
        for idx, (t, v) in enumerate(recent_measurements, start=start_idx):
            if self.include_timestamp:
                line = f"{idx}: {t} {v}"
            else:
                line = f"{idx}: {v}"
            dpg.add_text(line, parent="meas_container")
        
        if len(self.meas_history) > 0:
            dpg.set_y_scroll("meas_scroll", dpg.get_y_scroll_max("meas_scroll"))
    
    def _update_plot_axes(self):
        """Update plot axis limits"""
        if len(self.plot_y) == 0:
            return
        
        y_min = min(self.plot_y)
        y_max = max(self.plot_y)
        y_range = y_max - y_min
        
        if y_range < 0.001:
            margin = 0.1
        else:
            margin = y_range * 0.1
        
        dpg.set_axis_limits("y_axis", y_min - margin, y_max + margin)
        
        if len(self.plot_x) > 0:
            dpg.set_axis_limits("x_axis", min(self.plot_x) - 0.5, max(self.plot_x) + 0.5)
