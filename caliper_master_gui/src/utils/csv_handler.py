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
        self._include_timestamp: bool = False
        self._include_angle: bool = False

    @classmethod
    def _sanitize_prefix(cls, prefix: str) -> str:
        p = (prefix or "").strip()
        if p.lower().endswith(".csv"):
            p = p[:-4].strip()
        for ch in cls._INVALID_FILENAME_CHARS:
            p = p.replace(ch, "_")
        p = "_".join(p.split())
        return p or cls.DEFAULT_PREFIX

    def create_new_file(self, prefix: str = DEFAULT_PREFIX, include_timestamp: bool = False, include_angle: bool = False, calibration_offset: float = 0.0, reference: float = 0.0) -> str:
        if self.file:
            self.close()

        safe_prefix = self._sanitize_prefix(prefix)
        self.filename = datetime.now().strftime(f"{safe_prefix}_%Y%m%d_%H%M%S.csv")
        self.file = open(self.filename, "w", newline="")
        self.writer = csv.writer(self.file)
        self._include_timestamp = include_timestamp
        self._include_angle = include_angle

        self.writer.writerow([f"Offset: {calibration_offset:.3f}  Reference: {reference:.3f}"])

        columns = ["Index", "Value"]
        if self._include_angle:
            columns.append("Angle")
        if self._include_timestamp:
            columns.append("Timestamp")
        self.writer.writerow(columns)

        return self.filename

    def write_measurement(self, idx: int, value: str, angle: str = "", timestamp: str = ""):
        if not self.writer:
            return
        row = [idx, value]
        if self._include_angle:
            row.append(angle)
        if self._include_timestamp:
            row.append(timestamp)
        self.writer.writerow(row)

    def write_row(self, row: list):
        if self.writer:
            self.writer.writerow(row)

    def close(self):
        if self.file:
            self.file.close()
            self.file = None
            self.writer = None
            self.filename = None

    def get_filename(self) -> Optional[str]:
        return self.filename

    def is_open(self) -> bool:
        return self.file is not None

    def remove_last_row(self) -> bool:
        if not self.filename:
            return False

        saved_filename = self.filename
        if self.file:
            self.close()

        try:
            with open(saved_filename, "r", newline="") as f:
                lines = f.readlines()

            if len(lines) <= 2:
                return False

            with open(saved_filename, "w", newline="") as f:
                f.writelines(lines[:-1])

            self.file = open(saved_filename, "a", newline="")
            self.writer = csv.writer(self.file)
            self.filename = saved_filename
            return True
        except Exception:
            return False
