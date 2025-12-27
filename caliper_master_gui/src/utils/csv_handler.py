"""
CSV Handler for Caliper Master GUI
"""

import csv
from datetime import datetime
from typing import Optional, TextIO


class CSVHandler:
    """Handles CSV file operations for measurement data"""
    
    def __init__(self):
        self.file: Optional[TextIO] = None
        self.writer: Optional[csv.writer] = None
        self.filename: Optional[str] = None
        self.include_timestamp: bool = False
    
    def create_new_file(self, include_timestamp: bool = False) -> str:
        """Create a new CSV file with timestamp-based name"""
        if self.file:
            self.close()
        
        self.include_timestamp = include_timestamp
        self.filename = datetime.now().strftime("measurement_%Y%m%d_%H%M%S.csv")
        self.file = open(self.filename, "w", newline="")
        self.writer = csv.writer(self.file)
        
        return self.filename
    
    def write_measurement(self, measurement: str, timestamp: Optional[str] = None):
        """Write a measurement to the CSV file"""
        if not self.writer:
            return
        
        if self.include_timestamp:
            ts = timestamp or datetime.now().isoformat(timespec='seconds')
            self.writer.writerow([ts, measurement])
        else:
            self.writer.writerow([measurement])
    
    def write_row(self, row: list):
        """Write a custom row to the CSV file"""
        if self.writer:
            self.writer.writerow(row)
    
    def close(self):
        """Close the CSV file"""
        if self.file:
            self.file.close()
            self.file = None
            self.writer = None
    
    def get_filename(self) -> Optional[str]:
        """Get the current CSV filename"""
        return self.filename
    
    def is_open(self) -> bool:
        """Check if a CSV file is open"""
        return self.file is not None
