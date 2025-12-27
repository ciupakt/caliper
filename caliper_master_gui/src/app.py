"""
Caliper Master GUI - Main Application Class
"""

import threading
from collections import deque
from datetime import datetime


class CaliperApp:
    """Main application state and data management"""
    
    def __init__(self):
        self.ser = None
        self.auto_event = threading.Event()
        self.current_port = ''
        self.csv_file = None
        self.csv_writer = None
        self.timestamp_on = False
        self.auto_interval = 1000
        self.log_lines = deque(maxlen=200)  # Use deque for better performance
        self.meas_history = deque(maxlen=1000)  # Limit history size
        self.plot_x = deque(maxlen=500)  # Limit plot points for performance
        self.plot_y = deque(maxlen=500)
        self.log_tab_visible = False
        self.last_measurement_time = 0
        self.measurement_count = 0
    
    def add_log(self, line: str):
        """Add a line to the log"""
        self.log_lines.append(line)
    
    def add_measurement(self, timestamp: str, value: str):
        """Add a measurement to history"""
        self.meas_history.append((timestamp, value))
        self.measurement_count += 1
    
    def add_plot_point(self, x: int, y: float):
        """Add a point to the plot"""
        self.plot_x.append(x)
        self.plot_y.append(y)
    
    def clear_measurements(self):
        """Clear all measurements and plot data"""
        self.meas_history.clear()
        self.plot_x.clear()
        self.plot_y.clear()
        self.measurement_count = 0
    
    def close(self):
        """Close all resources"""
        if self.ser:
            self.ser.close()
        if self.csv_file:
            self.csv_file.close()
