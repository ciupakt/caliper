"""
CSV Handler for Caliper Master GUI
"""

import csv
from datetime import datetime
from typing import Optional, TextIO


class CSVHandler:
    """Handles CSV file operations for measurement data"""

    DEFAULT_PREFIX = "test"
    _INVALID_FILENAME_CHARS = '<>:"/\\|?*'

    def __init__(self):
        self.file: Optional[TextIO] = None
        self.writer: Optional[csv.writer] = None
        self.filename: Optional[str] = None

    @classmethod
    def _sanitize_prefix(cls, prefix: str) -> str:
        """Make a safe filename prefix for Windows."""
        p = (prefix or "").strip()
        if p.lower().endswith(".csv"):
            p = p[:-4].strip()

        # Replace invalid filename characters
        for ch in cls._INVALID_FILENAME_CHARS:
            p = p.replace(ch, "_")

        # Normalize whitespace
        p = "_".join(p.split())

        return p or cls.DEFAULT_PREFIX

    def create_new_file(self, prefix: str = DEFAULT_PREFIX) -> str:
        """Create a new CSV file with timestamp-based name.

        `prefix` replaces the default "measurement" prefix.
        Resulting filename format: <prefix>_YYYYMMDD_HHMMSS.csv
        """
        if self.file:
            self.close()

        safe_prefix = self._sanitize_prefix(prefix)
        self.filename = datetime.now().strftime(f"{safe_prefix}_%Y%m%d_%H%M%S.csv")
        self.file = open(self.filename, "w", newline="")
        self.writer = csv.writer(self.file)

        return self.filename
    
    def write_measurement(self, measurement: str, timestamp: Optional[str] = None, angle: Optional[str] = None):
        """Write a measurement to the CSV file
        
        Args:
            measurement: Measurement value string
            timestamp: Optional timestamp string. If provided, the row will include timestamp (at the end).
                      If None, only the measurement value is written.
            angle: Optional angle string. If provided, the row will include angle (after measurement).
        """
        if not self.writer:
            return
        
        # Build row based on what's included
        # Order: value, angle, timestamp
        row = [measurement]
        if angle is not None:
            row.append(angle)
        if timestamp is not None:
            ts = timestamp or datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            row.append(ts)
        
        self.writer.writerow(row)
    
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
            self.filename = None
    
    def get_filename(self) -> Optional[str]:
        """Get the current CSV filename"""
        return self.filename
    
    def is_open(self) -> bool:
        """Check if a CSV file is open"""
        return self.file is not None
