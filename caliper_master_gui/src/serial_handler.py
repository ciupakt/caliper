"""
Serial Port Handler for Caliper Master GUI
"""

import serial
import serial.tools.list_ports
import time
from typing import Optional, Callable


class SerialHandler:
    """Handles serial port communication"""
    
    def __init__(self, baud_rate: int = 115200, timeout: float = 0.2):
        self.baud_rate = baud_rate
        self.timeout = timeout
        self.ser: Optional[serial.Serial] = None
        self.current_port = ''
        self.running = False
        self.data_callback: Optional[Callable[[str], None]] = None
        self.write_callback: Optional[Callable[[str], None]] = None
    
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
    
    def start_reading(self):
        """Start background reading thread"""
        self.running = True
        import threading
        self.read_thread = threading.Thread(target=self._read_loop, daemon=True)
        self.read_thread.start()
    
    def stop_reading(self):
        """Stop background reading thread"""
        self.running = False
    
    def _read_loop(self):
        """Background reading loop"""
        while self.running:
            if self.is_open():
                data = self.read_line()
                if data and self.data_callback:
                    self.data_callback(data)
            time.sleep(0.02)
