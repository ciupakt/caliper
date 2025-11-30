import dearpygui.dearpygui as dpg
import serial
import serial.tools.list_ports
import threading
import time
import csv
from datetime import datetime
from collections import deque

class CaliperApp:
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

# Global app instance
app = CaliperApp()

def list_ports():
    return [port.device for port in serial.tools.list_ports.comports()]

def open_port_callback():
    port = dpg.get_value("port_combo")
    baud = 115200
    if app.ser:
        app.ser.close()
    try:
        app.ser = serial.Serial(port, baud, timeout=0.2)
        app.current_port = port
        dpg.set_value("status", f"Connected to {port}")
        fname = datetime.now().strftime("measurement_%Y%m%d_%H%M%S.csv")
        app.csv_file = open(fname, "w", newline="")
        app.csv_writer = csv.writer(app.csv_file)
        dpg.set_value("csv_info", f"CSV file: {fname}")
    except Exception as e:
        dpg.set_value("status", f"ERR: {str(e)}")
        app.ser = None

def close_all():
    if app.ser:
        app.ser.close()
    if app.csv_file:
        app.csv_file.close()

def update_ports():
    ports = list_ports()
    dpg.configure_item("port_combo", items=ports)
    if ports:
        dpg.set_value("port_combo", ports[0])

def send_command(command):
    """Unified command sending function"""
    if app.ser and app.ser.is_open:
        app.ser.write(f"{command}\n".encode())
        log_rx(f"> {command}")
    else:
        dpg.set_value("status", "Port not open!")

def send_trigger():
    send_command("m")

def send_motor_forward():
    send_command("f")

def send_motor_reverse():
    send_command("r")

def send_motor_stop():
    send_command("s")

def set_auto(sender, app_data):
    running = dpg.get_value("auto_checkbox")
    dpg.configure_item("interval_ms", enabled=running)
    if running:
        app.auto_event.clear()
        auto_thread = threading.Thread(target=auto_task, daemon=True)
        auto_thread.start()
    else:
        app.auto_event.set()

def auto_task():
    try:
        interval = int(dpg.get_value("interval_ms"))
        if interval < 500: interval = 500
        app.auto_interval = interval
    except:
        app.auto_interval = 1000
    while not app.auto_event.is_set():
        send_trigger()
        time.sleep(app.auto_interval/1000.0)

def timestamp_checkbox(sender, app_data):
    app.timestamp_on = app_data
    show_measurements()

def on_trigger():
    send_trigger()

def clear_measurements():
    app.meas_history.clear()
    app.plot_x.clear()
    app.plot_y.clear()
    app.measurement_count = 0
    dpg.set_value("plot_data", [list(app.plot_x), list(app.plot_y)])
    show_measurements()
    if app.csv_file:
        app.csv_file.close()
    fname = datetime.now().strftime("measurement_%Y%m%d_%H%M%S.csv")
    app.csv_file = open(fname, "w", newline="")
    app.csv_writer = csv.writer(app.csv_file)
    dpg.set_value("csv_info", f"CSV file: {fname}")
    dpg.set_value("status", "Measurements cleared, new CSV created")

def log_rx(line):
    app.log_lines.append(line)
    dpg.set_value("log_text", "\n".join(list(app.log_lines)))

def show_measurements():
    if dpg.does_item_exist("meas_container"):
        dpg.delete_item("meas_container", children_only=True)
    
    # Show only last 50 measurements for performance
    recent_measurements = list(app.meas_history)[-50:]
    start_idx = max(1, len(app.meas_history) - len(recent_measurements) + 1)
    
    for idx, (t, v) in enumerate(recent_measurements, start=start_idx):
        if app.timestamp_on:
            line = f"{idx}: {t} {v}"
        else:
            line = f"{idx}: {v}"
        dpg.add_text(line, parent="meas_container")
    
    if len(app.meas_history) > 0:
        dpg.set_y_scroll("meas_scroll", dpg.get_y_scroll_max("meas_scroll"))

def update_plot_axes():
    if len(app.plot_y) == 0:
        return
    
    y_min = min(app.plot_y)
    y_max = max(app.plot_y)
    y_range = y_max - y_min
    
    if y_range < 0.001:
        margin = 0.1
    else:
        margin = y_range * 0.1
    
    dpg.set_axis_limits("y_axis", y_min - margin, y_max + margin)
    
    if len(app.plot_x) > 0:
        dpg.set_axis_limits("x_axis", min(app.plot_x) - 0.5, max(app.plot_x) + 0.5)

def toggle_log_tab():
    app.log_tab_visible = not app.log_tab_visible
    if app.log_tab_visible:
        dpg.show_item("log_tab")
    else:
        dpg.hide_item("log_tab")

def key_press_handler(sender, key):
    # Check for Ctrl+Alt+L
    if key == dpg.mvKey_L:
        ctrl_pressed = dpg.is_key_down(dpg.mvKey_LControl) or dpg.is_key_down(dpg.mvKey_RControl)
        alt_pressed = dpg.is_key_down(dpg.mvKey_LAlt) or dpg.is_key_down(dpg.mvKey_RAlt)
        if ctrl_pressed and alt_pressed:
            toggle_log_tab()

def process_measurement_data(data):
    """Process measurement data with validation and storage"""
    try:
        val_str = data.split(":")[1]
        val = float(val_str)
        
        # Validate range
        if -1000.0 <= val <= 1000.0:
            ts = datetime.now().isoformat(timespec='seconds')
            measurement_str = f"{val_str} mm"
            app.meas_history.append((ts, measurement_str))
            app.measurement_count += 1
            
            # Update plot with rolling window
            app.plot_x.append(app.measurement_count)
            app.plot_y.append(val)
            
            # Update GUI elements
            dpg.set_value("plot_data", [list(app.plot_x), list(app.plot_y)])
            update_plot_axes()
            show_measurements()
            
            # Save to CSV
            if app.csv_writer:
                if app.timestamp_on:
                    app.csv_writer.writerow([ts, measurement_str])
                else:
                    app.csv_writer.writerow([measurement_str])
        else:
            log_rx(f"BLAD: Wartosc poza zakresem: {val}")
    except ValueError as val_err:
        log_rx(f"BLAD: Nieprawidlowa wartosc: {val_str} - {str(val_err)}")

def read_serial():
    while True:
        if app.ser and app.ser.is_open:
            try:
                data = app.ser.readline().decode(errors='ignore').strip()
                if data:
                    log_rx(f"< {data}")
                    
                    # Process measurement data
                    if data.startswith("VAL_1:"):
                        process_measurement_data(data)
                    elif "SILNIK" in data.upper() or "blad silnika" in data.lower():
                        log_rx(f"[SILNIK] {data}")
                        
                time.sleep(0.02)
            except Exception as e:
                dpg.set_value("status", f"ERR Serial: {str(e)}")
                log_rx(f"ERR Serial: {str(e)}")
                time.sleep(1)
        else:
            time.sleep(1)

dpg.create_context()

with dpg.font_registry():
    default_font = dpg.add_font("C:/Windows/Fonts/segoeui.ttf", 18)

# Register keyboard handler
with dpg.handler_registry():
    dpg.add_key_release_handler(callback=key_press_handler)

dpg.create_viewport(title="Caliper COM App", width=900, height=700)

with dpg.window(label="Caliper Application", width=880, height=660):
    with dpg.tab_bar():
        with dpg.tab(label="Measurement"):
            with dpg.group(horizontal=True):
                with dpg.group():
                    dpg.add_text("COM Port Configuration:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    ports_list = list_ports()
                    dpg.add_combo(ports_list, tag="port_combo", width=250)
                    if ports_list:
                        dpg.set_value("port_combo", ports_list[0])
                    dpg.add_spacer(height=5)
                    dpg.add_button(label="Refresh ports", callback=update_ports, width=150, height=30)
                    dpg.add_button(label="Open port", callback=open_port_callback, width=150, height=30)
                    dpg.add_spacer(height=5)
                    dpg.add_text("Status: Not connected", tag="status")
                    dpg.add_text("", tag="csv_info")
                
                dpg.add_spacer(width=30)
                
                with dpg.group():
                    dpg.add_text("Measurement Controls:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    dpg.add_button(label="Trigger measurement", callback=on_trigger, width=200, height=30)
                    dpg.add_button(label="Clear measurements", callback=clear_measurements, width=200, height=30)
                    dpg.add_spacer(height=5)
                    dpg.add_checkbox(label="Auto trigger", tag="auto_checkbox", callback=set_auto)
                    dpg.add_input_int(label="Interval (ms)", tag="interval_ms", default_value=1000, min_value=500, enabled=False, width=150)
                    dpg.add_spacer(height=5)
                    dpg.add_checkbox(label="Include timestamp", callback=timestamp_checkbox, tag="timestamp_cb")
                    dpg.add_spacer(height=10)
                    
                    dpg.add_text("Motor Controls:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    with dpg.group(horizontal=True):
                        dpg.add_button(label="Forward", callback=send_motor_forward, width=95, height=30)
                        dpg.add_button(label="Reverse", callback=send_motor_reverse, width=95, height=30)
                    dpg.add_button(label="Stop Motor", callback=send_motor_stop, width=200, height=30)
                    dpg.add_spacer(height=5)
                    dpg.add_text("ESP32 Master: http://192.168.4.1", color=(100, 255, 100))
                    dpg.add_text("Connect WiFi: ESP32_Pomiar", color=(100, 255, 100))
            
            dpg.add_separator()
            dpg.add_spacer(height=10)
            
            dpg.add_text("Measurement History:")
            with dpg.child_window(width=840, height=120, tag="meas_scroll"):
                dpg.add_group(tag="meas_container")
            
            dpg.add_spacer(height=10)
            dpg.add_text("Live Plot:")
            with dpg.plot(label="Measurements", height=250, width=840):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Measurement #", tag="x_axis")
                dpg.add_plot_axis(dpg.mvYAxis, label="Value", tag="y_axis")
                dpg.add_line_series([], [], label="Measurement", parent="y_axis", tag="plot_data")

        with dpg.tab(label="Log", tag="log_tab", show=False):
            dpg.add_spacer(height=5)
            dpg.add_text("Debug Log (Ctrl+Alt+L to toggle)", color=(255, 200, 100))
            dpg.add_spacer(height=5)
            dpg.add_input_text(multiline=True, readonly=True, width=840, height=560, tag="log_text")

dpg.bind_font(default_font)
threading.Thread(target=read_serial, daemon=True).start()

dpg.setup_dearpygui()
dpg.show_viewport()
dpg.start_dearpygui()
dpg.destroy_context()
close_all()
