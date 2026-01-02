"""
Log Tab GUI Component for Caliper Master GUI
"""

import dearpygui.dearpygui as dpg
from collections import deque


class LogTab:
    """Log tab component for displaying debug information"""

    def __init__(self, max_lines: int = 200):
        self.max_lines = max_lines
        self.log_lines = deque(maxlen=max_lines)
        self.visible = False

    def create(self, parent: int, serial_handler):
        """Create the log tab UI"""
        with dpg.tab(label="Log", tag="log_tab", show=False, parent=parent):
            dpg.add_spacer(height=5)
            dpg.add_text("Debug Log (Ctrl+Alt+L to toggle)", color=(255, 200, 100))
            dpg.add_spacer(height=5)

            dpg.add_text("Motor configuration (UART):", color=(100, 200, 255))
            with dpg.group(horizontal=True):
                dpg.add_input_int(
                    label="motorTorque",
                    tag="log_motor_torque",
                    default_value=0,
                    min_value=0,
                    max_value=255,
                    width=120,
                )
                dpg.add_input_int(
                    label="motorSpeed",
                    tag="log_motor_speed",
                    default_value=255,
                    min_value=0,
                    max_value=255,
                    width=120,
                )
                dpg.add_input_int(
                    label="motorState (0-3)",
                    tag="log_motor_state",
                    default_value=0,
                    min_value=0,
                    max_value=3,
                    width=120,
                )

            with dpg.group(horizontal=True):
                dpg.add_button(
                    label="Zastosuj",
                    callback=self._apply_motor_config,
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

            dpg.add_spacer(height=5)
            dpg.add_text("UART cmds: q <0-255>, s <0-255>, r <0-3>, t", color=(150, 150, 150))
            dpg.add_spacer(height=5)
            dpg.add_input_text(multiline=True, readonly=True, width=840, height=470, tag="log_text")
    
    def add_log(self, line: str):
        """Add a line to the log"""
        self.log_lines.append(line)
        if dpg.does_item_exist("log_text"):
            dpg.set_value("log_text", "\n".join(list(self.log_lines)))

    @staticmethod
    def _clamp_int(val: int, vmin: int, vmax: int) -> int:
        return max(vmin, min(vmax, int(val)))

    def _safe_write(self, serial_handler, data: str) -> bool:
        if serial_handler is None or not hasattr(serial_handler, "is_open"):
            self.add_log("[GUI] ERR: Serial handler missing")
            if dpg.does_item_exist("status"):
                dpg.set_value("status", "ERR: Serial handler missing")
            return False

        if not serial_handler.is_open():
            self.add_log("[GUI] ERR: Port not open")
            if dpg.does_item_exist("status"):
                dpg.set_value("status", "ERR: Port not open")
            return False

        serial_handler.write(data)
        return True

    def _apply_motor_config(self, sender, app_data, user_data):
        """Apply msgMaster.motorTorque/motorSpeed/motorState via UART: q, s, r."""
        serial_handler = user_data

        try:
            torque = int(dpg.get_value("log_motor_torque"))
            speed = int(dpg.get_value("log_motor_speed"))
            state = int(dpg.get_value("log_motor_state"))
        except Exception:
            self.add_log("[GUI] ERR: Invalid motor config values")
            return

        torque = self._clamp_int(torque, 0, 255)
        speed = self._clamp_int(speed, 0, 255)
        state = self._clamp_int(state, 0, 3)

        dpg.set_value("log_motor_torque", torque)
        dpg.set_value("log_motor_speed", speed)
        dpg.set_value("log_motor_state", state)

        # Order matters: set torque/speed first, then r <state> (firmware sends motor test on 'r')
        if not self._safe_write(serial_handler, f"q {torque}"):
            return
        self._safe_write(serial_handler, f"s {speed}")
        self._safe_write(serial_handler, f"r {state}")

        if dpg.does_item_exist("status"):
            dpg.set_value("status", f"Sent: q {torque}, s {speed}, r {state}")
        self.add_log(f"[GUI] Sent: q {torque}, s {speed}, r {state}")

    def _send_motortest(self, sender, app_data, user_data):
        """Send motor test command via UART: t."""
        serial_handler = user_data
        if self._safe_write(serial_handler, "t"):
            if dpg.does_item_exist("status"):
                dpg.set_value("status", "Sent: t")
            self.add_log("[GUI] Sent: t")
    
    def toggle_visibility(self):
        """Toggle log tab visibility"""
        self.visible = not self.visible
        if dpg.does_item_exist("log_tab"):
            if self.visible:
                dpg.show_item("log_tab")
            else:
                dpg.hide_item("log_tab")
    
    def clear(self):
        """Clear all log lines"""
        self.log_lines.clear()
        if dpg.does_item_exist("log_text"):
            dpg.set_value("log_text", "")
