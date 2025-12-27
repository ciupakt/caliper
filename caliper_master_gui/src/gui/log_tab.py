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
    
    def create(self, parent: int):
        """Create the log tab UI"""
        with dpg.tab(label="Log", tag="log_tab", show=False, parent=parent):
            dpg.add_spacer(height=5)
            dpg.add_text("Debug Log (Ctrl+Alt+L to toggle)", color=(255, 200, 100))
            dpg.add_spacer(height=5)
            dpg.add_input_text(multiline=True, readonly=True, width=840, height=560, tag="log_text")
    
    def add_log(self, line: str):
        """Add a line to the log"""
        self.log_lines.append(line)
        if dpg.does_item_exist("log_text"):
            dpg.set_value("log_text", "\n".join(list(self.log_lines)))
    
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
