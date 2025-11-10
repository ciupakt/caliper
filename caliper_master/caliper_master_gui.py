import dearpygui.dearpygui as dpg
import serial
import serial.tools.list_ports
import threading
import time
import csv
from datetime import datetime

ser = None
auto_event = threading.Event()
current_port = ''
csv_file = None
csv_writer = None
timestamp_on = False
auto_interval = 1000
log_lines = []
meas_history = []
plot_x = []
plot_y = []
log_tab_visible = False

def list_ports():
    return [port.device for port in serial.tools.list_ports.comports()]

def open_port_callback():
    global ser, current_port, csv_file, csv_writer
    port = dpg.get_value("port_combo")
    baud = 115200
    if ser:
        ser.close()
    try:
        ser = serial.Serial(port, baud, timeout=0.2)
        current_port = port
        dpg.set_value("status", f"Connected to {port}")
        fname = datetime.now().strftime("measurement_%Y%m%d_%H%M%S.csv")
        csv_file = open(fname, "w", newline="")
        csv_writer = csv.writer(csv_file)
        dpg.set_value("csv_info", f"CSV file: {fname}")
    except Exception as e:
        dpg.set_value("status", f"ERR: {str(e)}")
        ser = None

def close_all():
    global ser, csv_file
    if ser: ser.close()
    if csv_file: csv_file.close()

def update_ports():
    ports = list_ports()
    dpg.configure_item("port_combo", items=ports)
    if ports:
        dpg.set_value("port_combo", ports[0])

def send_trigger():
    if ser and ser.is_open:
        ser.write(b"m\n")
        log_rx("> m")
    else:
        dpg.set_value("status", "Port not open!")

def set_auto(sender, app_data):
    running = dpg.get_value("auto_checkbox")
    dpg.configure_item("interval_ms", enabled=running)
    if running:
        global auto_thread, auto_event
        auto_event.clear()
        auto_thread = threading.Thread(target=auto_task, daemon=True)
        auto_thread.start()
    else:
        auto_event.set()

def auto_task():
    global auto_interval
    try:
        interval = int(dpg.get_value("interval_ms"))
        if interval < 500: interval = 500
        auto_interval = interval
    except:
        auto_interval = 1000
    while not auto_event.is_set():
        send_trigger()
        time.sleep(auto_interval/1000.0)

def timestamp_checkbox(sender, app_data):
    global timestamp_on
    timestamp_on = app_data
    show_measurements()

def on_trigger():
    send_trigger()

def clear_measurements():
    global meas_history, csv_file, csv_writer, plot_x, plot_y
    meas_history.clear()
    plot_x.clear()
    plot_y.clear()
    dpg.set_value("plot_data", [plot_x, plot_y])
    show_measurements()
    if csv_file:
        csv_file.close()
    fname = datetime.now().strftime("measurement_%Y%m%d_%H%M%S.csv")
    csv_file = open(fname, "w", newline="")
    csv_writer = csv.writer(csv_file)
    dpg.set_value("csv_info", f"CSV file: {fname}")
    dpg.set_value("status", "Measurements cleared, new CSV created")

def log_rx(line):
    log_lines.append(line)
    dpg.set_value("log_text", "\n".join(log_lines[-200:]))

def show_measurements():
    if dpg.does_item_exist("meas_container"):
        dpg.delete_item("meas_container", children_only=True)
    
    for idx, (t, v) in enumerate(meas_history, start=1):
        if timestamp_on:
            line = f"{idx}: {t} {v}"
        else:
            line = f"{idx}: {v}"
        dpg.add_text(line, parent="meas_container")
    
    if len(meas_history) > 0:
        dpg.set_y_scroll("meas_scroll", dpg.get_y_scroll_max("meas_scroll"))

def update_plot_axes():
    if len(plot_y) == 0:
        return
    
    y_min = min(plot_y)
    y_max = max(plot_y)
    y_range = y_max - y_min
    
    if y_range < 0.001:
        margin = 0.1
    else:
        margin = y_range * 0.1
    
    dpg.set_axis_limits("y_axis", y_min - margin, y_max + margin)
    
    if len(plot_x) > 0:
        dpg.set_axis_limits("x_axis", min(plot_x) - 0.5, max(plot_x) + 0.5)

def toggle_log_tab():
    global log_tab_visible
    log_tab_visible = not log_tab_visible
    if log_tab_visible:
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

def read_serial():
    while True:
        if ser and ser.is_open:
            try:
                data = ser.readline().decode(errors='ignore').strip()
                if data:
                    log_rx(f"< {data}")
                    if data.startswith("VAL_1:"):
                        val = data.split(":")[1]
                        ts = datetime.now().isoformat(timespec='seconds')
                        meas_history.append((ts, val))
                        show_measurements()
                        try:
                            plot_x.append(len(plot_x) + 1)
                            plot_y.append(float(val))
                            dpg.set_value("plot_data", [plot_x, plot_y])
                            update_plot_axes()
                        except:
                            pass
                        if csv_writer:
                            if timestamp_on:
                                csv_writer.writerow([ts, val])
                            else:
                                csv_writer.writerow([val])
                time.sleep(0.02)
            except Exception as e:
                dpg.set_value("status", f"ERR Serial: {str(e)}")
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
                    dpg.add_text("", tag="status")
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
