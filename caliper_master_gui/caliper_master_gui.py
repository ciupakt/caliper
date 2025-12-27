"""
Caliper Master GUI - Main Application Entry Point
Refactored modular version
"""

import dearpygui.dearpygui as dpg
import threading
import time
from datetime import datetime

# Import application modules
from src.app import CaliperApp
from src.serial_handler import SerialHandler
from src.utils.csv_handler import CSVHandler
from src.gui.measurement_tab import MeasurementTab
from src.gui.log_tab import LogTab


class CaliperGUI:
    """Main GUI application class"""
    
    def __init__(self):
        self.app = CaliperApp()
        self.serial_handler = SerialHandler()
        self.csv_handler = CSVHandler()
        self.measurement_tab = MeasurementTab()
        self.log_tab = LogTab()
        self.auto_event = threading.Event()
        self.auto_running = False
    
    def process_measurement_data(self, data: str):
        """Process measurement data with validation and storage"""
        try:
            # Handle calibration data
            if data.startswith("CAL_OFFSET:"):
                val_str = data.split(":")[1]
                self.log_tab.add_log(f"[KALIBRACJA] Offset: {val_str}")
                return
            
            if data.startswith("CAL_ERROR:"):
                val_str = data.split(":")[1]
                self.log_tab.add_log(f"[KALIBRACJA] Błąd: {val_str} mm")
                return
            
            # Handle measurement session data
            if data.startswith("MEAS_SESSION:"):
                parts = data.split(" ", 2)
                if len(parts) >= 3:
                    session_name = parts[1]
                    val_str = parts[2]
                    self.log_tab.add_log(f"[SESJA {session_name}] Pomiar: {val_str} mm")
                    
                    # Write to CSV with session name
                    if self.csv_handler.is_open():
                        ts = datetime.now().isoformat(timespec='seconds')
                        measurement_str = f"{val_str} mm"
                        if self.measurement_tab.include_timestamp:
                            self.csv_handler.write_row([ts, f"[{session_name}] {measurement_str}"])
                        else:
                            self.csv_handler.write_row([f"[{session_name}] {measurement_str}"])
                return
            
            # Handle VAL_1 measurements
            if data.startswith("VAL_1:"):
                val_str = data.split(":")[1]
                val = float(val_str)
                
                # Validate range
                if -1000.0 <= val <= 1000.0:
                    ts = datetime.now().isoformat(timespec='seconds')
                    measurement_str = f"{val_str} mm"
                    
                    # Add to measurement tab
                    self.measurement_tab.add_measurement(ts, measurement_str, val)
                    
                    # Save to CSV
                    if self.csv_handler.is_open():
                        if self.measurement_tab.include_timestamp:
                            self.csv_handler.write_measurement(measurement_str, ts)
                        else:
                            self.csv_handler.write_measurement(measurement_str)
                else:
                    self.log_tab.add_log(f"BLAD: Wartosc poza zakresem: {val}")
            
            # Handle angle data
            elif data.startswith(">Angle X:"):
                angle_str = data.split(":")[1]
                self.log_tab.add_log(f"[ANGLE X] {angle_str}°")
            
            # Handle battery data
            elif data.startswith(">Napiecie baterii:"):
                voltage_str = data.split(":")[1]
                self.log_tab.add_log(f"[BATERIA] {voltage_str}")
            
            # Handle motor data
            elif "SILNIK" in data.upper() or "blad silnika" in data.lower():
                self.log_tab.add_log(f"[SILNIK] {data}")
        
        except ValueError as val_err:
            self.log_tab.add_log(f"BLAD: Nieprawidlowa wartosc - {str(val_err)}")
        except Exception as e:
            self.log_tab.add_log(f"BLAD przetwarzania danych: {str(e)}")
    
    def serial_data_callback(self, data: str):
        """Callback for received serial data"""
        self.log_tab.add_log(f"< {data}")
        
        # Process measurement data
        if (data.startswith("VAL_1:") or data.startswith("CAL_OFFSET:") or 
            data.startswith("CAL_ERROR:") or data.startswith("MEAS_SESSION:")):
            self.process_measurement_data(data)
        elif data.startswith(">Angle X:") or data.startswith(">Napiecie baterii:"):
            self.process_measurement_data(data)
        elif "SILNIK" in data.upper() or "blad silnika" in data.lower():
            self.log_tab.add_log(f"[SILNIK] {data}")
    
    def auto_task(self):
        """Auto trigger task"""
        try:
            interval = int(dpg.get_value("interval_ms"))
            if interval < 500:
                interval = 500
        except:
            interval = 1000
        
        while not self.auto_event.is_set():
            self.serial_handler.write("m")
            time.sleep(interval / 1000.0)
    
    def key_press_handler(self, sender, key):
        """Handle keyboard shortcuts"""
        # Check for Ctrl+Alt+L
        if key == dpg.mvKey_L:
            ctrl_pressed = dpg.is_key_down(dpg.mvKey_LControl) or dpg.is_key_down(dpg.mvKey_RControl)
            alt_pressed = dpg.is_key_down(dpg.mvKey_LAlt) or dpg.is_key_down(dpg.mvKey_RAlt)
            if ctrl_pressed and alt_pressed:
                self.log_tab.toggle_visibility()
    
    def create_gui(self):
        """Create the main GUI"""
        dpg.create_context()
        
        # Font registry
        with dpg.font_registry():
            default_font = dpg.add_font("C:/Windows/Fonts/segoeui.ttf", 18)
        
        # Handler registry
        with dpg.handler_registry():
            dpg.add_key_release_handler(callback=self.key_press_handler)
        
        # Create viewport
        dpg.create_viewport(title="Caliper COM App", width=900, height=700)

        # Main window
        with dpg.window(label="Caliper Application", width=880, height=660):
            # Uwaga: `dpg.tab` MUSI mieć jako rodzica `mvTabBar`.
            # Nie polegamy na `dpg.last_container()` (potrafi wskazać ostatnio utworzony kontener,
            # a nie aktualny `tab_bar`), tylko przekazujemy jawnie identyfikator/tab tag.
            with dpg.tab_bar(tag="main_tab_bar") as tab_bar_id:
                # Measurement tab
                self.measurement_tab.create(tab_bar_id, self.serial_handler, self.csv_handler)

                # Log tab
                self.log_tab.create(tab_bar_id)

        # Bind font
        dpg.bind_font(default_font)
    
    def run(self):
        """Run the application"""
        # Set up serial data callback
        self.serial_handler.set_data_callback(self.serial_data_callback)
        
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
