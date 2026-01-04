"""caliper_master_gui.src.gui.measurement_tab

Zakładka „Pomiary” (GUI).
"""

import dearpygui.dearpygui as dpg
from collections import deque
from datetime import datetime
import threading
import time
import re


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
    
    def create(self, parent: int, serial_handler, csv_handler):
        """Create the measurement tab UI"""
        with dpg.tab(label="Pomiary", parent=parent):
            # Układ 3-kolumnowy:
            # - lewa kolumna: historia pomiarów (żeby było widać więcej wpisów)
            # - środek: sterowanie pomiarem (pozostaje "po środku")
            # - prawa kolumna: konfiguracja portu COM (przy prawej krawędzi okna)
            with dpg.group(horizontal=True):
                # --- Measurement History (left column)
                with dpg.group():
                    dpg.add_text("Historia pomiarów:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    # Większa wysokość = więcej widocznych pomiarów bez scrollowania
                    # (wyższa kolumna = więcej widocznych pomiarów)
                    with dpg.child_window(width=420, height=360, tag="meas_scroll"):
                        dpg.add_group(tag="meas_container")

                dpg.add_spacer(width=30)

                # --- Measurement Controls (center column)
                with dpg.group():
                    dpg.add_text("Sterowanie pomiarem:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)

                    # Główny przycisk: większy + czerwony, pogrubiony napis
                    measure_btn = dpg.add_button(
                        label="Wykonaj pomiar (p)",
                        callback=self._trigger,
                        width=200,
                        height=55,
                        user_data=serial_handler,
                    )

                    # Theme (kolor tekstu)
                    if not dpg.does_item_exist("measure_button_theme"):
                        with dpg.theme(tag="measure_button_theme"):
                            with dpg.theme_component(dpg.mvButton):
                                dpg.add_theme_color(dpg.mvThemeCol_Text, (0, 200, 0, 255))

                    dpg.bind_item_theme(measure_btn, "measure_button_theme")

                    # Font (pogrubienie) – zdefiniowany w [`CaliperGUI.create_gui()`](caliper_master_gui/caliper_master_gui.py:219)
                    if dpg.does_item_exist("font_bold"):
                        dpg.bind_item_font(measure_btn, "font_bold")

                    # Nazewnictwo jak w WWW: nowa sesja pomiarowa.
                    # Plik CSV tworzymy z nazwą sesji jako prefixem.
                    new_session_btn = dpg.add_button(label="Nowa sesja pomiarowa", width=200, height=30)
                    with dpg.popup(new_session_btn, mousebutton=dpg.mvMouseButton_Left, modal=True, tag="new_session_popup"):
                        dpg.add_text("Podaj nazwę sesji (maks 31 znaków, dozwolone: a-z, A-Z, 0-9, spacja, _, -)")
                        dpg.add_text("Zostanie utworzony plik: <nazwa_sesji>_YYYYMMDD_HHMMSS.csv")
                        dpg.add_spacer(height=5)
                        dpg.add_input_text(tag="session_name_input", default_value=self.session_name, width=360)
                        dpg.add_spacer(height=8)
                        with dpg.group(horizontal=True):
                            dpg.add_button(label="Utwórz", callback=self._confirm_new_session, width=120, height=30, user_data=(serial_handler, csv_handler))
                            dpg.add_button(label="Anuluj", callback=lambda: dpg.configure_item("new_session_popup", show=False), width=120, height=30)

                    dpg.add_spacer(height=5)
                    dpg.add_text("", tag="session_name_display")

                    dpg.add_spacer(height=5)
                    dpg.add_checkbox(label="Auto-pomiar", tag="auto_checkbox", callback=self._set_auto, user_data=serial_handler)
                    dpg.add_input_int(label="Interwał (ms)", tag="interval_ms", default_value=1000, min_value=500, enabled=False, width=150)
                    dpg.add_spacer(height=5)
                    dpg.add_checkbox(label="Dodaj znacznik czasu", callback=self._timestamp_checkbox, tag="timestamp_cb")
                    dpg.add_checkbox(label="Dodaj znacznik kąta", callback=self._angle_checkbox, tag="angle_cb")

                dpg.add_spacer(width=30)

                # --- Port Configuration (right column)
                with dpg.group():
                    dpg.add_text("Konfiguracja portu COM:", color=(100, 200, 255))
                    dpg.add_spacer(height=5)
                    ports_list = serial_handler.list_ports()
                    dpg.add_combo(ports_list, tag="port_combo", width=250)
                    if ports_list:
                        dpg.set_value("port_combo", ports_list[0])
                    dpg.add_spacer(height=5)
                    dpg.add_button(label="Odśwież porty", callback=self._refresh_ports, width=150, height=30, user_data=serial_handler)
                    dpg.add_button(label="Otwórz port", callback=self._open_port, width=150, height=30, user_data=(serial_handler, csv_handler))
                    dpg.add_spacer(height=5)
                    dpg.add_text("Status: brak połączenia", tag="status")
                    dpg.add_text("", tag="csv_info")

            dpg.add_separator()
            dpg.add_spacer(height=10)

            # Live Plot
            dpg.add_text("Wykres na żywo:")
            with dpg.plot(label="Pomiary", height=260, width=1120):
                dpg.add_plot_legend()
                dpg.add_plot_axis(dpg.mvXAxis, label="Nr pomiaru", tag="x_axis")
                dpg.add_plot_axis(dpg.mvYAxis, label="Wartość", tag="y_axis")
                dpg.add_line_series([], [], label="Pomiar", parent="y_axis", tag="plot_data")
    
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
            dpg.set_value("status", f"Połączono z {port}")

            # Zgodnie z wymaganiem: plik CSV NIE jest tworzony przy otwieraniu portu.
            # Jeśli był otwarty poprzedni plik, zamykamy go, żeby nowa sesja zawsze
            # tworzyła nowy plik po podaniu prefixu.
            try:
                if csv_handler is not None and hasattr(csv_handler, "is_open") and csv_handler.is_open():
                    csv_handler.close()
            except Exception:
                pass

            dpg.set_value("csv_info", "Plik CSV: (brak — kliknij 'Nowa sesja pomiarowa')")
        else:
            dpg.set_value("status", "BŁĄD: Nie udało się otworzyć portu")
    
    def _trigger(self, sender, app_data, user_data):
        """Send trigger command"""
        serial_handler = user_data
        serial_handler.write("m")
    
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
            try:
                if dpg.does_item_exist("status"):
                    dpg.set_value("status", "BŁĄD: Nazwa sesji jest nieprawidłowa")
            except Exception:
                pass
            return

        self.session_name = session_name

        # Wysyłanie komendy 'n' do ESP32 Master
        try:
            if serial_handler is not None and hasattr(serial_handler, "is_open") and serial_handler.is_open():
                serial_handler.write(f"n {session_name}")
                # Zapisz do logu aplikacji (przez callback w main)
            else:
                try:
                    if dpg.does_item_exist("status"):
                        dpg.set_value("status", "BŁĄD: Port nie jest otwarty")
                except Exception:
                    pass
                return
        except Exception:
            try:
                if dpg.does_item_exist("status"):
                    dpg.set_value("status", "BŁĄD: Nie udało się wysłać nazwy sesji")
            except Exception:
                pass
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
            dpg.set_value("csv_info", f"Plik CSV: {filename}")
            dpg.set_value("status", f"Nowa sesja pomiarowa: {session_name}")
        else:
            dpg.set_value("status", "BŁĄD: Nie udało się utworzyć pliku CSV")

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
            # szybka informacja w statusie (UI thread)
            try:
                if dpg.does_item_exist("status"):
                    if serial_handler is None or not hasattr(serial_handler, "is_open") or not serial_handler.is_open():
                        dpg.set_value("status", "Auto-pomiar: włączony (port nieotwarty)")
                    else:
                        dpg.set_value("status", "Auto-pomiar: włączony")
            except Exception:
                pass

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

            try:
                if dpg.does_item_exist("status"):
                    dpg.set_value("status", "Auto-pomiar: wyłączony")
            except Exception:
                pass

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
        allowed_pattern = r'^[a-zA-Z0-9 _-]+$'
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
                if serial_handler is not None and hasattr(serial_handler, "is_open") and serial_handler.is_open():
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
    
    def add_measurement(self, timestamp: str, value: str, numeric_value: float, angle: str = ""):
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
