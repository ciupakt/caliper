"""
Serial Port Handler for Caliper Master GUI
"""

import serial
import serial.tools.list_ports
import threading
import time
from typing import Optional, Callable


class SerialHandler:
    """Handles serial port communication"""

    # Keys expected in the 'g' (refresh settings) response from Master.
    # All 7 settings must be received to confirm a valid connection.
    PROBE_KEYS = frozenset({
        "calibrationOffset", "reference", "timeout",
        "motorTorque", "motorSpeed", "motorState", "sessionName",
    })

    def __init__(self, baud_rate: int = 115200, timeout: float = 0.2):
        self.baud_rate = baud_rate
        self.timeout = timeout
        self.ser: Optional[serial.Serial] = None
        self.current_port = ''
        self.running = False
        self.data_callback: Optional[Callable[[str], None]] = None
        self.write_callback: Optional[Callable[[str], None]] = None

        # Auto-connect probe state (coordinated with _read_loop)
        self._probe_active = threading.Event()
        self._probe_complete = threading.Event()
        self._probe_received: set[str] = set()
        self._auto_connect_stop = threading.Event()
    
    @staticmethod
    def list_ports() -> list:
        """List available serial ports"""
        return [port.device for port in serial.tools.list_ports.comports()]
    
    def open_port(self, port: str) -> bool:
        """Open a serial port"""
        try:
            if self.ser and self.ser.is_open:
                self.ser.close()
            
            self.ser = serial.Serial(port, self.baud_rate, timeout=self.timeout)
            self.current_port = port
            return True
        except Exception as e:
            print(f"Error opening port: {e}")
            return False
    
    def close_port(self):
        """Close the serial port"""
        if self.ser and self.ser.is_open:
            self.ser.close()
    
    def is_open(self) -> bool:
        """Check if port is open"""
        return self.ser is not None and self.ser.is_open
    
    def write(self, data: str):
        """Write data to serial port"""
        if self.is_open():
            self.ser.write(f"{data}\n".encode())
            # Call write callback if set
            if self.write_callback:
                self.write_callback(data)
    
    def read_line(self) -> Optional[str]:
        """Read a line from serial port"""
        if self.is_open():
            try:
                data = self.ser.readline().decode(errors='ignore').strip()
                return data if data else None
            except Exception as e:
                print(f"Error reading from port: {e}")
        return None
    
    def set_data_callback(self, callback: Callable[[str], None]):
        """Set callback for received data"""
        self.data_callback = callback
    
    def set_write_callback(self, callback: Callable[[str], None]):
        """Set callback for written data"""
        self.write_callback = callback
    
    def _feed_probe(self, data: str) -> None:
        """Track probe response keys. Called from _read_loop for each received line.

        Collects settings keys from DEBUG_PLOT lines (prefixed with '>') until all
        PROBE_KEYS are received, then signals _probe_complete.
        """
        if self._probe_active.is_set() and data.startswith('>'):
            key = data[1:].split(':', 1)[0].strip()
            if key in self.PROBE_KEYS:
                self._probe_received.add(key)
                if self.PROBE_KEYS.issubset(self._probe_received):
                    self._probe_complete.set()

    def start_reading(self):
        """Start background reading thread"""
        self.running = True
        self.read_thread = threading.Thread(target=self._read_loop, daemon=True)
        self.read_thread.start()

    def stop_reading(self):
        """Stop background reading thread"""
        self.running = False
        self._probe_active.clear()

    def _read_loop(self):
        """Background reading loop"""
        while self.running:
            if self.is_open():
                data = self.read_line()
                if data:
                    self._feed_probe(data)
                    if self.data_callback:
                        self.data_callback(data)
            time.sleep(0.02)

    def stop_auto_connect(self):
        """Signal auto_connect loop to stop (interruptible, non-blocking)."""
        self._auto_connect_stop.set()

    def auto_connect(self, status_callback=None, probe_command='g',
                     settle_delay=0.3, retries=3, per_attempt_timeout=1.2,
                     rescan_delay=2.0, max_rounds=None) -> bool:
        """Scan COM ports and connect to the first that responds to probe_command
        with all settings (DEBUG_PLOT lines prefixed with '>').

        After exhausting all currently-available ports, re-lists ports and starts
        a new iteration — this catches ports that appear in the system during
        scanning (e.g. a device plugged in mid-scan). Loops until success or
        until stop_auto_connect() is called.

        Uses retry within timeout — robust against ESP32 reset-on-open (DTR) which
        requires ~1-2s boot before the Master can answer.

        Args:
            status_callback: optional callback payload=(display_text, connected_port|None)
            probe_command: command sent to probe the device (default 'g')
            settle_delay: seconds to wait after opening port (DTR reset / Serial start)
            retries: max number of probe attempts per port
            per_attempt_timeout: seconds to wait for a complete response per attempt
            rescan_delay: seconds to wait between scan rounds when no new ports found
            max_rounds: max scan rounds after which to give up (None = infinite)

        Returns:
            True if a port was successfully connected, False otherwise.
        """
        tried: set[str] = set()
        rounds_without_new = 0

        while not self._auto_connect_stop.is_set():
            ports = self.list_ports()
            candidates = [p for p in ports if p not in tried]

            if not candidates:
                rounds_without_new += 1
                if status_callback:
                    status_callback(("No COM ports" if not ports else "Scanning...", None))
                if max_rounds is not None and rounds_without_new >= max_rounds:
                    break
                if self._auto_connect_stop.wait(rescan_delay):
                    break
                tried.clear()
                continue

            for port in candidates:
                if self._auto_connect_stop.is_set():
                    break
                tried.add(port)
                if status_callback:
                    status_callback((f"Connecting {port}...", None))
                if not self.open_port(port):
                    if status_callback:
                        status_callback((f"{port} busy", None))
                    continue
                time.sleep(settle_delay)
                try:
                    self.ser.reset_input_buffer()
                except Exception:
                    pass
                self._probe_received.clear()
                self._probe_complete.clear()
                self._probe_active.set()
                connected = False
                for _ in range(retries):
                    if self._auto_connect_stop.is_set():
                        break
                    try:
                        self.write(probe_command)
                    except Exception:
                        break
                    if self._probe_complete.wait(timeout=per_attempt_timeout):
                        connected = True
                        break
                self._probe_active.clear()
                if connected:
                    if status_callback:
                        status_callback((port, port))
                    return True
                self.close_port()
                if status_callback:
                    status_callback((f"No response {port}", None))

        if status_callback and not self.is_open():
            status_callback(("(none)", None))
        return False
