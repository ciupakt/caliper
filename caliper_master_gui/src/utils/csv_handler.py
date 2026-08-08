"""
CSV Handler for Caliper Master GUI
"""

import csv
import threading
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
        self._lock = threading.Lock()

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
        with self._lock:
            if self.file:
                self._close_unlocked()

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

    def rewrite(self, measurements, calibration_offset: float = 0.0, reference: float = 0.0, include_timestamp: bool = False, include_angle: bool = False) -> bool:
        """Rewrite the whole CSV file from the given measurement list.

        Called whenever the measurement history changes (add/drop) so the file
        on disk always reflects the current GUI state. No-op when no session
        file is open.

        Args:
            measurements: iterable of (timestamp, value, angle) tuples
            calibration_offset: current calibration offset (header)
            reference: current reference (header)
            include_timestamp: whether to emit the Timestamp column
            include_angle: whether to emit the Angle column

        Returns:
            True if the file was rewritten successfully.
        """
        with self._lock:
            if not self.filename:
                return False

            saved_filename = self.filename
            self._close_unlocked()

            try:
                with open(saved_filename, "w", newline="") as f:
                    writer = csv.writer(f)
                    writer.writerow([f"Offset: {calibration_offset:.3f}  Reference: {reference:.3f}"])

                    columns = ["Index", "Value"]
                    if include_angle:
                        columns.append("Angle")
                    if include_timestamp:
                        columns.append("Timestamp")
                    writer.writerow(columns)

                    for idx, (t, v, a) in enumerate(measurements, start=1):
                        row = [idx, v]
                        if include_angle:
                            row.append(a)
                        if include_timestamp:
                            row.append(t)
                        writer.writerow(row)

                self.file = open(saved_filename, "a", newline="")
                self.writer = csv.writer(self.file)
                self._include_timestamp = include_timestamp
                self._include_angle = include_angle
                self.filename = saved_filename
                return True
            except Exception:
                self.file = None
                self.writer = None
                self.filename = None
                return False

    def write_row(self, row: list):
        if self.writer:
            self.writer.writerow(row)

    def _close_unlocked(self):
        """Close the file handle without acquiring the lock (caller holds it)."""
        if self.file:
            self.file.close()
            self.file = None
            self.writer = None

    def close(self):
        with self._lock:
            self._close_unlocked()
            self.filename = None

    def get_filename(self) -> Optional[str]:
        return self.filename

    def is_open(self) -> bool:
        return self.file is not None
