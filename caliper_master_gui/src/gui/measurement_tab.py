"""caliper_master_gui.src.gui.measurement_tab

Zakładka „Pomiary” (GUI).
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

        # Domyślny prefix plików CSV (zamiennik „measurement_”)
        self.csv_prefix: str = "test"

        # Nazwa sesji (używana jako domyślna wartość w polu input)
        self.session_name: str = ""

        # Auto-pomiar (wątek wysyłający cyklicznie komendę "m")
        self._auto_event = threading.Event()
        self._auto_thread: threading.Thread | None = None

        # Referencje ustawiane w create()
        self._csv_handler = None
        self._on_drop = None

    def create(self, parent: int, serial_handler, csv_handler, on_drop=None):
        """Create the measurement tab UI

        Args:
            on_drop: opcjonalny callback wywoływany po anulowaniu ostatniego
                     pomiaru (np. do synchronizacji zakładki Gauge).
        """
        self._csv_handler = csv_handler
        self._on_drop = on_drop
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

                # --- Port Config (right column, width=288)
                with dpg.group(width=288):
                    dpg.add_text("COM Port Configuration:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    ports_list = serial_handler.list_ports()
                    dpg.add_combo(ports_list, tag="port_combo", width=288)
                    if ports_list:
                        dpg.set_value("port_combo", ports_list[0])
                    dpg.add_spacer(height=5)
                    dpg.add_button(
                        label="Refresh Ports",
                        callback=self._refresh_ports,
                        width=288,
                        height=30,
                        user_data=serial_handler,
                    )
                    dpg.add_spacer(height=5)
                    dpg.add_button(
                        label="Open Port",
                        callback=self._open_port,
                        width=288,
                        height=30,
                        user_data=(serial_handler, csv_handler),
                    )
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

                # Elastyczny odstęp dociskający "Connected to" do prawej krawędzi.
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
        """Otwórz katalog zawierający bieżący plik CSV w menedżerze plików."""
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
            if sys.platform.startswith("win"):
                subprocess.Popen(["explorer", "/select,", abs_path])
            elif sys.platform.startswith("darwin"):
                subprocess.Popen(["open", "-R", abs_path])
            else:
                subprocess.Popen(["xdg-open", directory])
        except Exception:
            pass

    def _save_session_as(self, sender, app_data, user_data):
        """Otwórz natywne okno 'Zapisz jako' i skopiuj/przenieś plik CSV."""
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
            """Zwraca wybraną ścieżkę lub pusty string przy anulowaniu."""
            if sys.platform.startswith("win"):
                # Windows – natywne okno Win32 przez PowerShell
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
                # Linux – zenity (natywne GTK)
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
            # Dodaj rozszerzenie .csv jeśli brak
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
        """Dopasuj szerokość spacera w wierszu statusu, aby "Connected to:"
        było wyrównane do prawej krawędzi okna.

        Działa również poprawnie po zmianie rozmiaru okna oraz po zmianie
        długości nazwy sesji / nazwy portu COM.
        """
        try:
            if not dpg.does_item_exist("status_row_spacer"):
                return
            if not dpg.does_item_exist("status_row"):
                return

            # Dostępna szerokość obszaru zakładki – używamy szerokości viewportu
            # pomniejszonej o typowy padding okna głównego i obszaru zakładek.
            try:
                avail_w = int(dpg.get_viewport_client_width()) - 32
            except Exception:
                avail_w = 1100
            if avail_w <= 0:
                avail_w = 1100

            # Lista dzieci wiersza statusu
            try:
                children = dpg.get_item_children("status_row", 1) or []
            except Exception:
                children = []

            # Znajdź indeks spacera
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

            # Suma szerokości elementów po lewej i prawej stronie spacera
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

            # Item spacing w grupie horizontal (domyślny styl Dear PyGui ~8 px).
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
        """Ustaw label przycisku csv_info pełną nazwą pliku.

        Nazwa sesji jest ograniczona do 31 znaków przy tworzeniu nowej sesji,
        więc pełna nazwa pliku zmieści się w wierszu statusu bez skracania.
        """
        display = filename if filename else "(none)"
        try:
            if dpg.does_item_exist("csv_info"):
                dpg.configure_item("csv_info", label=display)
            if dpg.does_item_exist("csv_info_tooltip"):
                dpg.set_value("csv_info_tooltip", display)
        except Exception:
            pass

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

        # Jeśli port jest już otwarty i kliknięto "Open Port" dla tego samego portu,
        # nie ruszamy bieżącej sesji (zachowujemy nazwę pliku CSV za "Session:").
        try:
            already_open_same_port = (
                serial_handler is not None
                and hasattr(serial_handler, "is_open")
                and serial_handler.is_open()
                and getattr(serial_handler, "current_port", None) == port
            )
        except Exception:
            already_open_same_port = False

        if already_open_same_port:
            dpg.set_value("port_status", port)
            return

        if serial_handler.open_port(port):
            dpg.set_value("port_status", port)

            # Zgodnie z wymaganiem: plik CSV NIE jest tworzony przy otwieraniu portu.
            # Jeśli był otwarty poprzedni plik, zamykamy go, żeby nowa sesja zawsze
            # tworzyła nowy plik po podaniu prefixu.
            try:
                if (
                    csv_handler is not None
                    and hasattr(csv_handler, "is_open")
                    and csv_handler.is_open()
                ):
                    csv_handler.close()
            except Exception:
                pass

            self._set_csv_info_label(None)
        else:
            dpg.set_value("port_status", "(none)")

    def _trigger(self, sender, app_data, user_data):
        """Send trigger command"""
        serial_handler = user_data
        serial_handler.write("m")

    def _cancel_last_measurement(self, sender=None, app_data=None, user_data=None):
        """Anuluj ostatni pomiar: usuń z historii/wykresu, z pliku CSV i odśwież Gauge."""
        if self.drop_last_measurement():
            try:
                if self._csv_handler is not None and self._csv_handler.is_open():
                    self._csv_handler.remove_last_row()
            except Exception:
                pass

        # Synchronizacja zakładki Gauge (np. pokaż poprzedni pomiar lub wyczyść)
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

        # Walidacja nazwy sesji
        if not self._validate_session_name(session_name):
            return

        self.session_name = session_name

        # Wysyłanie komendy 'n' do ESP32 Master
        try:
            if (
                serial_handler is not None
                and hasattr(serial_handler, "is_open")
                and serial_handler.is_open()
            ):
                serial_handler.write(f"n {session_name}")
                # Zapisz do logu aplikacji (przez callback w main)
            else:
                return
        except Exception:
            return

        # Użyj nazwy sesji jako prefixu pliku CSV
        self.csv_prefix = session_name

        # Clear GUI measurements
        self._clear()

        # Create CSV file now
        filename = None
        try:
            filename = csv_handler.create_new_file(prefix=self.csv_prefix)
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
            # jeśli już działa, nic nie rób
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
            # join krótki, żeby nie blokować GUI przy długim sleep
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
    def _validate_session_name(name: str) -> bool:
        """Walidacja nazwy sesji.

        Args:
            name: Nazwa sesji do walidacji

        Returns:
            True jeśli nazwa jest prawidłowa
        """
        # Minimalna długość: 1 znak
        if not name or len(name) < 1:
            return False

        # Maksymalna długość: 31 znaków
        if len(name) > 31:
            return False

        # Dozwolone znaki: litery (a-z, A-Z), cyfry (0-9), spacje, podkreślenia (_), myślniki (-)
        allowed_pattern = r"^[a-zA-Z0-9 _-]+$"
        if not re.match(allowed_pattern, name):
            return False

        return True

    def _auto_loop(self, serial_handler):
        """Worker loop for auto-measure.

        Uwaga: unikamy operacji na UI z wątku w tle (DearPyGui nie gwarantuje thread-safety).
        """
        while not self._auto_event.is_set():
            # odczytuj interwał „na żywo”, żeby zmiana w UI działała bez restartu
            try:
                interval = int(dpg.get_value("interval_ms"))
            except Exception:
                interval = 1000

            interval = self._clamp_int(interval, 500, 600000)

            # wysyłamy tylko gdy port otwarty; jeśli nie, po prostu czekamy
            try:
                if (
                    serial_handler is not None
                    and hasattr(serial_handler, "is_open")
                    and serial_handler.is_open()
                ):
                    serial_handler.write("m")
            except Exception:
                # nie wywalaj wątku – najwyżej pomiń iterację
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

        # Pokazujemy więcej wpisów (historia jest teraz wysoką kolumną po lewej stronie)
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
