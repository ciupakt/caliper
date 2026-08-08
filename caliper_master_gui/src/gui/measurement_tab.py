"""caliper_master_gui.src.gui.measurement_tab

"Measurements" tab (GUI).
"""

import dearpygui.dearpygui as dpg
from collections import deque
from datetime import datetime
import threading
import time
import re
import threading
import os
import subprocess
import sys


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
        self.include_angle = False

        self.calibration_offset: float = 0.0
        self.reference: float = 0.0

        # Default CSV file prefix (replacement for "measurement_")
        self.csv_prefix: str = "test"

        # Session name (used as default value in input field)
        self.session_name: str = ""

# Auto-measure (thread sending command "m" cyclically)
        self._auto_event = threading.Event()
        self._auto_thread: threading.Thread | None = None

        # References set in create()
        self._csv_handler = None
        self._on_drop = None
        self._on_simulate = None

    def create(self, parent: int, serial_handler, csv_handler, on_drop=None, on_simulate=None):
        """Create the measurement tab UI

        Args:
            on_drop: optional callback invoked after canceling last
                     measurement (e.g. to sync Gauge tab).
            on_simulate: optional callback that returns True if simulation
                        was handled (no serial write needed).
        """
        self._csv_handler = csv_handler
        self._on_drop = on_drop
        self._on_simulate = on_simulate
        with dpg.tab(label="Measurements", parent=parent):
            with dpg.group(horizontal=True):
                # --- Measurement History (left column)
                with dpg.group():
                    dpg.add_text("Measurement History:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    with dpg.child_window(width=420, height=360, tag="meas_scroll"):
                        dpg.add_group(tag="meas_container")

                dpg.add_spacer(width=30)

                # --- Controls (center column, width=288)
                with dpg.group(width=288):
                    dpg.add_text("Measurement Controls:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    measure_btn = dpg.add_button(
                        label="Measure (m)",
                        callback=self._trigger,
                        width=288,
                        height=55,
                        user_data=serial_handler,
                    )

                    if not dpg.does_item_exist("measure_button_theme"):
                        with dpg.theme(tag="measure_button_theme"):
                            with dpg.theme_component(dpg.mvButton):
                                dpg.add_theme_color(
                                    dpg.mvThemeCol_Text, (0, 200, 0, 255)
                                )

                    dpg.bind_item_theme(measure_btn, "measure_button_theme")

                    if dpg.does_item_exist("font_bold"):
                        dpg.bind_item_font(measure_btn, "font_bold")

                    dpg.add_spacer(height=5)
                    dpg.add_button(
                        label="Cancel last measurement",
                        callback=self._cancel_last_measurement,
                        width=288,
                        height=30,
                    )

                    dpg.add_spacer(height=5)
                    dpg.add_checkbox(
                        label="Include timestamp",
                        callback=self._timestamp_checkbox,
                        tag="timestamp_cb",
                    )
                    dpg.add_checkbox(
                        label="Include angle",
                        callback=self._angle_checkbox,
                        tag="angle_cb",
                    )
                    dpg.add_spacer(height=5)
                    dpg.add_checkbox(
                        label="Auto-measure",
                        tag="auto_checkbox",
                        callback=self._set_auto,
                        user_data=serial_handler,
                    )
                    dpg.add_text("Interval (ms)")
                    dpg.add_input_int(
                        tag="interval_ms",
                        default_value=1000,
                        min_value=500,
                        enabled=False,
                        width=150,
                    )

                dpg.add_spacer(width=30)

                # --- Session / Reference (right column, width=288)
                with dpg.group(width=288):
                    dpg.add_text("Session:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    new_session_btn = dpg.add_button(
                        label="New Session", width=288, height=30
                    )
                    dpg.add_spacer(height=5)
                    dpg.add_button(
                        label="Save session as:",
                        callback=self._save_session_as,
                        width=288,
                        height=30,
                        user_data=csv_handler,
                    )
                    dpg.add_spacer(height=5)
                    dpg.add_text("Reference (mm)")
                    dpg.add_input_float(
                        tag="ref_input_meas",
                        default_value=0.0,
                        min_value=-999.999,
                        max_value=999.999,
                        format="%.3f",
                        width=288,
                    )

                    with dpg.popup(
                        new_session_btn,
                        mousebutton=dpg.mvMouseButton_Left,
                        modal=True,
                        tag="new_session_popup",
                    ):
                        dpg.add_text(
                            "Enter session name (max 31 chars, allowed: a-z, A-Z, 0-9, space, _, -)"
                        )
                        dpg.add_text(
                            "File will be created: <session_name>_YYYYMMDD_HHMMSS.csv"
                        )
                        dpg.add_spacer(height=5)
                        dpg.add_input_text(
                            tag="session_name_input",
                            default_value=self.session_name,
                            width=360,
                            on_enter=True,
                            callback=self._confirm_new_session,
                            user_data=(serial_handler, csv_handler),
                        )
                        dpg.add_spacer(height=8)
                        with dpg.group(horizontal=True):
                            dpg.add_button(
                                label="Create",
                                callback=self._confirm_new_session,
                                width=120,
                                height=30,
                                user_data=(serial_handler, csv_handler),
                            )
                            dpg.add_button(
                                label="Cancel",
                                callback=lambda: dpg.configure_item(
                                    "new_session_popup", show=False
                                ),
                                width=120,
                                height=30,
                            )

            dpg.add_separator()
            dpg.add_spacer(height=10)

            # Status row: Session (left) + Connected to (right)
            with dpg.group(horizontal=True, tag="status_row"):
                dpg.add_text("Session:")
                csv_link_btn = dpg.add_button(
                    label="(none)",
                    tag="csv_info",
                    callback=self._open_csv_directory,
                    user_data=csv_handler,
                    small=True,
                )
                with dpg.tooltip("csv_info"):
                    dpg.add_text("(none)", tag="csv_info_tooltip")
                if not dpg.does_item_exist("csv_link_theme"):
                    with dpg.theme(tag="csv_link_theme"):
                        with dpg.theme_component(dpg.mvButton):
                            dpg.add_theme_color(dpg.mvThemeCol_Text, (100, 180, 255, 255))
                            dpg.add_theme_color(dpg.mvThemeCol_Button, (0, 0, 0, 0))
                            dpg.add_theme_color(dpg.mvThemeCol_ButtonHovered, (0, 0, 0, 0))
                            dpg.add_theme_color(dpg.mvThemeCol_ButtonActive, (0, 0, 0, 0))
                dpg.bind_item_theme(csv_link_btn, "csv_link_theme")

                # Flexible spacer pushing "Connected to" to the right edge.
                dpg.add_spacer(width=10, tag="status_row_spacer")
                dpg.add_text("Connected to:")
                dpg.add_text("(none)", tag="port_status")

            dpg.add_spacer(height=4)

            # Live Plot
            with dpg.plot(label="Measurements", height=-1, width=-1, tag="measurement_plot"):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Measurement #", tag="x_axis")
                dpg.add_plot_axis(dpg.mvYAxis, label="Value", tag="y_axis")
                dpg.add_line_series(
                    [], [], label="Measurement", parent="y_axis", tag="plot_data"
                )

    def _open_csv_directory(self, sender, app_data, user_data):
        """Open the directory containing the current CSV file in file manager."""
        csv_handler = user_data
        filename = None
        try:
            if csv_handler is not None and hasattr(csv_handler, "get_filename"):
                filename = csv_handler.get_filename()
        except Exception:
            filename = None

        if not filename:
            return

        abs_path = os.path.abspath(filename)
        directory = os.path.dirname(abs_path)

        try:
            # Hide the console window of the spawned helper process on Windows.
            creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
            if sys.platform.startswith("win"):
                subprocess.Popen(["explorer", "/select,", abs_path], creationflags=creationflags)
            elif sys.platform.startswith("darwin"):
                subprocess.Popen(["open", "-R", abs_path])
            else:
                subprocess.Popen(["xdg-open", directory])
        except Exception:
            pass

    def _save_session_as(self, sender, app_data, user_data):
        """Open native "Save as" window and copy/move the CSV file."""
        csv_handler = user_data
        src_filename = None
        try:
            if csv_handler is not None and hasattr(csv_handler, "get_filename"):
                src_filename = csv_handler.get_filename()
        except Exception:
            src_filename = None

        if not src_filename:
            return

        abs_src = os.path.abspath(src_filename)
        initial_dir = os.path.dirname(abs_src)
        initial_file = os.path.basename(abs_src)

        def _native_save_dialog() -> str:
            """Returns selected path or empty string on cancel."""
            # On Windows, hide the console window of the spawned helper process
            # (PowerShell) so "Save as" doesn't pop up a black cmd window.
            creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
            if sys.platform.startswith("win"):
                # Windows – native Win32 dialog via PowerShell
                ps_script = (
                    "Add-Type -AssemblyName System.Windows.Forms; "
                    "$d = New-Object System.Windows.Forms.SaveFileDialog; "
                    f"$d.InitialDirectory = '{initial_dir}'; "
                    f"$d.FileName = '{initial_file}'; "
                    "$d.Filter = 'CSV files (*.csv)|*.csv|All files (*.*)|*.*'; "
                    "$d.Title = 'Save session as'; "
                    "if ($d.ShowDialog() -eq 'OK') {{ Write-Output $d.FileName }}"
                )
                try:
                    result = subprocess.run(
                        ["powershell", "-NoProfile", "-Command", ps_script],
                        capture_output=True, text=True, timeout=120,
                        creationflags=creationflags,
                    )
                    return result.stdout.strip() if result.returncode == 0 else ""
                except Exception:
                    return ""
            elif sys.platform.startswith("darwin"):
                # macOS – natywny dialog przez osascript
                script = (
                    f'tell app "System Events" to '
                    f'POSIX path of (choose file name '
                    f'with prompt "Save session as:" '
                    f'default name "{initial_file}" '
                    f'default location POSIX file "{initial_dir}")'
                )
                try:
                    result = subprocess.run(
                        ["osascript", "-e", script],
                        capture_output=True, text=True, timeout=120,
                    )
                    return result.stdout.strip() if result.returncode == 0 else ""
                except Exception:
                    return ""
            else:
                # Linux – zenity (native GTK)
                try:
                    result = subprocess.run(
                        [
                            "zenity", "--file-selection",
                            "--save",
                            "--confirm-overwrite",
                            f"--filename={os.path.join(initial_dir, initial_file)}",
                            "--file-filter=CSV files (*.csv) | *.csv",
                            "--file-filter=All files | *",
                            "--title=Save session as",
                        ],
                        capture_output=True, text=True, timeout=120,
                    )
                    return result.stdout.strip() if result.returncode == 0 else ""
                except FileNotFoundError:
                    return ""
                except Exception:
                    return ""

        def _do_save_as():
            import shutil
            dest = _native_save_dialog()
            if not dest:
                return
            # Add .csv extension if missing
            if not dest.lower().endswith(".csv"):
                dest += ".csv"
            try:
                if csv_handler is not None and hasattr(csv_handler, "file") and csv_handler.file:
                    csv_handler.file.flush()
                shutil.copy2(abs_src, dest)
            except Exception:
                pass

        threading.Thread(target=_do_save_as, daemon=True).start()

    @staticmethod
    def update_status_row_layout() -> None:
        """Adjust spacer width in status row so "Connected to:"
        is aligned to the right edge of the window.

        Works correctly after window resize and after
        change in session name / COM port name length.
        """
        try:
            if not dpg.does_item_exist("status_row_spacer"):
                return
            if not dpg.does_item_exist("status_row"):
                return

            # Available tab area width – we use viewport width
            # minus typical main window and tab area padding.
            try:
                avail_w = int(dpg.get_viewport_client_width()) - 32
            except Exception:
                avail_w = 1100
            if avail_w <= 0:
                avail_w = 1100

            # List of status row children
            try:
                children = dpg.get_item_children("status_row", 1) or []
            except Exception:
                children = []

            # Find spacer index
            spacer_index = None
            for idx, child in enumerate(children):
                try:
                    if dpg.get_item_alias(child) == "status_row_spacer":
                        spacer_index = idx
                        break
                except Exception:
                    pass
            if spacer_index is None:
                return

            # Sum of widths of elements on left and right side of spacer
            left_w = 0
            for child in children[:spacer_index]:
                try:
                    w = dpg.get_item_rect_size(child)[0]
                    left_w += int(w)
                except Exception:
                    pass
            right_w = 0
            for child in children[spacer_index + 1 :]:
                try:
                    w = dpg.get_item_rect_size(child)[0]
                    right_w += int(w)
                except Exception:
                    pass

            # Item spacing in horizontal group (Dear PyGui default style ~8 px).
            ITEM_SPACING = 8
            num_gaps = max(len(children) - 1, 0)
            gaps_w = num_gaps * ITEM_SPACING

            spacer_w = avail_w - left_w - right_w - gaps_w
            if spacer_w < 10:
                spacer_w = 10

            dpg.configure_item("status_row_spacer", width=spacer_w)
        except Exception:
            pass

    @staticmethod
    def _set_csv_info_label(filename: str | None) -> None:
        """Set csv_info button label to full filename.

        Session name is limited to 31 characters when creating a new session,
        so the full filename fits in the status row without truncation.
        """
        display = filename if filename else "(none)"
        try:
            if dpg.does_item_exist("csv_info"):
                dpg.configure_item("csv_info", label=display)
            if dpg.does_item_exist("csv_info_tooltip"):
                dpg.set_value("csv_info_tooltip", display)
        except Exception:
            pass

    def _trigger(self, sender, app_data, user_data):
        """Send trigger command"""
        serial_handler = user_data
        if self._on_simulate is not None and self._on_simulate():
            return
        serial_handler.write("m")

    def _cancel_last_measurement(self, sender=None, app_data=None, user_data=None):
        """Cancel last measurement: remove from history/chart, from CSV file, and refresh Gauge."""
        if self.drop_last_measurement():
            try:
                if self._csv_handler is not None and self._csv_handler.is_open():
                    self._csv_handler.remove_last_row()
            except Exception:
                pass

        # Gauge tab sync (e.g. show previous measurement or clear)
        if callable(self._on_drop):
            try:
                self._on_drop()
            except Exception:
                pass

    def _clear(self, sender=None, app_data=None, user_data=None):
        """Clear all measurements (local GUI state only)."""
        self.meas_history.clear()
        self.plot_x.clear()
        self.plot_y.clear()
        self.measurement_count = 0
        dpg.set_value("plot_data", [list(self.plot_x), list(self.plot_y)])
        self._show_measurements()

    def _confirm_new_session(self, sender, app_data, user_data):
        """Create new CSV file with session name as prefix and clear history."""
        serial_handler, csv_handler = user_data

        # Read session name
        try:
            session_name = str(dpg.get_value("session_name_input")).strip()
        except Exception:
            session_name = ""

        # Session name validation
        if not self._validate_session_name(session_name):
            return

        self.session_name = session_name

        # Sending command 'n' to ESP32 Master
        try:
            if (
                serial_handler is not None
                and hasattr(serial_handler, "is_open")
                and serial_handler.is_open()
            ):
                serial_handler.write(f"n {session_name}")
                # Sending reference after creating new session
                try:
                    ref_val = float(dpg.get_value("ref_input_meas"))
                    ref_val = self._clamp_float(ref_val, -999.999, 999.999)
                    dpg.set_value("ref_input_meas", ref_val)
                    serial_handler.write(f"v {ref_val:.3f}")
                except Exception:
                    pass
                # Save to application log (via callback in main)
            else:
                return
        except Exception:
            return

        # Use session name as CSV file prefix
        self.csv_prefix = session_name

        # Clear GUI measurements
        self._clear()

        # Create CSV file now
        filename = None
        try:
            filename = csv_handler.create_new_file(
                prefix=self.csv_prefix,
                include_timestamp=self.include_timestamp,
                include_angle=self.include_angle,
                calibration_offset=self.calibration_offset,
                reference=self.reference,
            )
        except Exception:
            filename = None

        if filename:
            self._set_csv_info_label(os.path.basename(filename))

        try:
            dpg.configure_item("new_session_popup", show=False)
        except Exception:
            pass

    def _set_auto(self, sender, app_data, user_data):
        """Toggle auto trigger"""
        serial_handler = user_data
        running = dpg.get_value("auto_checkbox")
        dpg.configure_item("interval_ms", enabled=running)

        # Start / stop background task
        if running:
            # if already running, do nothing
            if self._auto_thread is not None and self._auto_thread.is_alive():
                return

            self._auto_event.clear()
            self._auto_thread = threading.Thread(
                target=self._auto_loop,
                args=(serial_handler,),
                daemon=True,
                name="auto_measurement",
            )
            self._auto_thread.start()
        else:
            self._auto_event.set()
            # short join to not block GUI on long sleep
            try:
                if self._auto_thread is not None:
                    self._auto_thread.join(timeout=0.3)
            except Exception:
                pass
            self._auto_thread = None

    def _clamp_int(self, value: int, min_val: int, max_val: int) -> int:
        """Clamp integer value to specified range."""
        return max(min_val, min(value, max_val))

    @staticmethod
    def _clamp_float(val: float, vmin: float, vmax: float) -> float:
        return max(vmin, min(vmax, float(val)))

    @staticmethod
    def _validate_session_name(name: str) -> bool:
        """Session name validation.

        Args:
            name: Session name to validate

        Returns:
            True if name is valid
        """
        # Minimum length: 1 character
        if not name or len(name) < 1:
            return False

        # Maximum length: 31 characters
        if len(name) > 31:
            return False

        # Allowed characters: letters (a-z, A-Z), digits (0-9), spaces, underscores (_), hyphens (-)
        allowed_pattern = r"^[a-zA-Z0-9 _-]+$"
        if not re.match(allowed_pattern, name):
            return False

        return True

    def _auto_loop(self, serial_handler):
        """Worker loop for auto-measure.

        Note: we avoid UI operations from background thread (DearPyGui does not guarantee thread-safety).
        """
        while not self._auto_event.is_set():
            # read interval "live" so that UI change works without restart
            try:
                interval = int(dpg.get_value("interval_ms"))
            except Exception:
                interval = 1000

            interval = self._clamp_int(interval, 500, 600000)

            # send only when port is open; if not, just wait
            try:
                if (
                    serial_handler is not None
                    and hasattr(serial_handler, "is_open")
                    and serial_handler.is_open()
                ):
                    if self._on_simulate is not None and self._on_simulate():
                        pass
                    else:
                        serial_handler.write("m")
            except Exception:
            # don’t crash the thread – at worst skip iteration
                pass

            time.sleep(interval / 1000.0)

    def _timestamp_checkbox(self, sender, app_data, user_data):
        """Toggle timestamp inclusion"""
        self.include_timestamp = app_data
        self._show_measurements()

    def _angle_checkbox(self, sender, app_data, user_data):
        """Toggle angle inclusion"""
        self.include_angle = app_data
        self._show_measurements()

    def add_measurement(
        self, timestamp: str, value: str, numeric_value: float, angle: str = ""
    ):
        """Add a measurement to history and plot

        Args:
            timestamp: Timestamp string
            value: Measurement value string
            numeric_value: Numeric value for plotting
            angle: Angle string (optional)
        """
        self.meas_history.append((timestamp, value, angle))
        self.measurement_count += 1

        # Update plot
        self.plot_x.append(self.measurement_count)
        self.plot_y.append(numeric_value)

        # Update GUI
        dpg.set_value("plot_data", [list(self.plot_x), list(self.plot_y)])
        self._update_plot_axes()
        self._show_measurements()

    def drop_last_measurement(self):
        """Remove the most recent measurement from history and plot"""
        if not self.meas_history:
            return False

        self.meas_history.pop()
        if self.plot_x:
            self.plot_x.pop()
        if self.plot_y:
            self.plot_y.pop()
        if self.measurement_count > 0:
            self.measurement_count -= 1

        dpg.set_value("plot_data", [list(self.plot_x), list(self.plot_y)])
        self._update_plot_axes()
        self._show_measurements()
        return True

    def _show_measurements(self):
        """Display measurements in the history view"""
        if dpg.does_item_exist("meas_container"):
            dpg.delete_item("meas_container", children_only=True)

        dpg.add_text(
            f"Offset: {self.calibration_offset:.3f}   Reference: {self.reference:.3f}",
            color=(200, 200, 100),
            parent="meas_container",
        )

        columns = ["Index", "Value"]
        if self.include_angle:
            columns.append("Angle")
        if self.include_timestamp:
            columns.append("Timestamp")
        dpg.add_text("  ".join(columns), color=(180, 180, 180), parent="meas_container")

        recent_measurements = list(self.meas_history)[-200:]
        start_idx = max(1, len(self.meas_history) - len(recent_measurements) + 1)

        for idx, (t, v, a) in enumerate(recent_measurements, start=start_idx):
            parts = [v]  # Always show measurement first
            if self.include_angle:
                parts.append(a)
            if self.include_timestamp:
                parts.append(t)
            line = f"{idx}: {' '.join(parts)}"
            dpg.add_text(line, parent="meas_container")

        if len(self.meas_history) > 0:

            def _autoscroll():
                try:
                    if dpg.does_item_exist("meas_scroll"):
                        dpg.set_y_scroll(
                            "meas_scroll", dpg.get_y_scroll_max("meas_scroll")
                        )
                except Exception:
                    pass

            threading.Timer(0.05, _autoscroll).start()

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
            dpg.set_axis_limits(
                "x_axis", min(self.plot_x) - 0.5, max(self.plot_x) + 0.5
            )
