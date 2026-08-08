"""
Unit tests for SerialHandler
"""

import unittest
from unittest.mock import Mock, patch, call
import threading
import sys
import os

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from serial_handler import SerialHandler


# Sample DEBUG_PLOT lines from Master 'g' response (matches user capture)
SAMPLE_RESPONSE_LINES = [
    ">calibrationOffset:-10.000",
    ">reference:0.000",
    ">timeout:1000",
    ">motorTorque:100",
    ">motorSpeed:100",
    ">motorState:0",
    ">sessionName:",
]


class TestSerialHandler(unittest.TestCase):
    """Test cases for SerialHandler class"""
    
    def setUp(self):
        """Set up test fixtures"""
        self.handler = SerialHandler()
    
    def test_list_ports(self):
        """Test listing available ports"""
        ports = SerialHandler.list_ports()
        self.assertIsInstance(ports, list)
    
    def test_initial_state(self):
        """Test initial handler state"""
        self.assertFalse(self.handler.is_open())
        self.assertIsNone(self.handler.ser)
        self.assertEqual(self.handler.current_port, '')
    
    def test_callback_setting(self):
        """Test setting data callback"""
        def dummy_callback(data):
            pass
        
        self.handler.set_data_callback(dummy_callback)
        self.assertIsNotNone(self.handler.data_callback)
    
    @patch('serial_handler.serial.Serial')
    def test_open_port_success(self, mock_serial):
        """Test successful port opening"""
        mock_serial_instance = Mock()
        mock_serial.return_value = mock_serial_instance
        mock_serial_instance.is_open = True
        
        result = self.handler.open_port('COM1')
        
        self.assertTrue(result)
        self.assertEqual(self.handler.current_port, 'COM1')
        mock_serial.assert_called_once_with('COM1', 115200, timeout=0.2)
    
    @patch('serial_handler.serial.Serial')
    def test_open_port_failure(self, mock_serial):
        """Test port opening failure"""
        mock_serial.side_effect = Exception("Port not found")
        
        result = self.handler.open_port('COM999')
        
        self.assertFalse(result)
        self.assertEqual(self.handler.current_port, '')


class TestFeedProbe(unittest.TestCase):
    """Tests for _feed_probe key-tracking logic (no running thread needed)"""

    def setUp(self):
        self.handler = SerialHandler()
        self.handler._probe_active.set()

    def test_full_set_completes(self):
        """Feeding all 7 keys signals _probe_complete."""
        for line in SAMPLE_RESPONSE_LINES:
            self.handler._feed_probe(line)
        self.assertTrue(self.handler._probe_complete.is_set())
        self.assertEqual(self.handler._probe_received, SerialHandler.PROBE_KEYS)

    def test_partial_does_not_complete(self):
        """Feeding only 2 keys does not complete."""
        self.handler._feed_probe(">calibrationOffset:-10.000")
        self.handler._feed_probe(">reference:0.000")
        self.assertFalse(self.handler._probe_complete.is_set())
        self.assertEqual(len(self.handler._probe_received), 2)

    def test_non_plot_lines_ignored(self):
        """Lines without '>' prefix or unknown keys are ignored."""
        self.handler._feed_probe("boot message")
        self.handler._feed_probe(">unknownKey:123")
        self.assertFalse(self.handler._probe_complete.is_set())
        self.assertEqual(self.handler._probe_received, set())

    def test_empty_session_name(self):
        """sessionName with empty value is still recognized."""
        self.handler._feed_probe(">sessionName:")
        self.assertIn("sessionName", self.handler._probe_received)

    def test_inactive_probe_ignores_all(self):
        """When probe is not active, lines are ignored."""
        self.handler._probe_active.clear()
        for line in SAMPLE_RESPONSE_LINES:
            self.handler._feed_probe(line)
        self.assertFalse(self.handler._probe_complete.is_set())
        self.assertEqual(self.handler._probe_received, set())

    def test_accumulates_across_calls(self):
        """Keys accumulate across multiple _feed_probe calls."""
        for line in SAMPLE_RESPONSE_LINES[:4]:
            self.handler._feed_probe(line)
        self.assertFalse(self.handler._probe_complete.is_set())
        for line in SAMPLE_RESPONSE_LINES[4:]:
            self.handler._feed_probe(line)
        self.assertTrue(self.handler._probe_complete.is_set())


class TestAutoConnect(unittest.TestCase):
    """Tests for auto_connect method"""

    def setUp(self):
        self.handler = SerialHandler()

    @patch('serial_handler.serial.Serial')
    def test_success(self, mock_serial):
        """Successful auto-connect: write triggers _probe_complete."""
        mock_serial_instance = Mock()
        mock_serial.return_value = mock_serial_instance
        mock_serial_instance.is_open = True

        with patch.object(self.handler, 'list_ports', return_value=['COM1']), \
             patch.object(self.handler, 'close_port') as mock_close:
            def fake_write(data):
                for line in SAMPLE_RESPONSE_LINES:
                    self.handler._feed_probe(line)
            with patch.object(self.handler, 'write', side_effect=fake_write):
                result = self.handler.auto_connect(
                    settle_delay=0, retries=1, per_attempt_timeout=0.5, max_rounds=1
                )
        self.assertTrue(result)
        self.assertEqual(self.handler.current_port, 'COM1')
        mock_close.assert_not_called()

    @patch('serial_handler.serial.Serial')
    def test_no_response(self, mock_serial):
        """No probe response: port closed, returns False."""
        mock_serial_instance = Mock()
        mock_serial.return_value = mock_serial_instance
        mock_serial_instance.is_open = True

        with patch.object(self.handler, 'list_ports', return_value=['COM1']), \
             patch.object(self.handler, 'write'), \
             patch.object(self.handler, 'close_port') as mock_close:
            result = self.handler.auto_connect(
                settle_delay=0, retries=1, per_attempt_timeout=0.05, max_rounds=1
            )
        self.assertFalse(result)
        mock_close.assert_called_once()

    @patch('serial_handler.serial.Serial')
    def test_open_port_failure_skips(self, mock_serial):
        """Port open failure: skips to next port."""
        mock_serial_instance = Mock()
        mock_serial.return_value = mock_serial_instance
        mock_serial_instance.is_open = True

        with patch.object(self.handler, 'list_ports', return_value=['COM1']), \
             patch.object(self.handler, 'open_port', return_value=False), \
             patch.object(self.handler, 'close_port') as mock_close:
            result = self.handler.auto_connect(
                settle_delay=0, retries=1, per_attempt_timeout=0.05, max_rounds=1
            )
        self.assertFalse(result)
        mock_close.assert_not_called()

    def test_no_ports(self):
        """No COM ports available: returns False with status callback."""
        statuses = []
        with patch.object(self.handler, 'list_ports', return_value=[]):
            result = self.handler.auto_connect(
                status_callback=statuses.append, max_rounds=1, rescan_delay=0.01
            )
        self.assertFalse(result)
        self.assertIn(("No COM ports", None), statuses)

    @patch('serial_handler.serial.Serial')
    def test_retry_succeeds_on_second_attempt(self, mock_serial):
        """Retry catches a delayed response on the 2nd attempt."""
        mock_serial_instance = Mock()
        mock_serial.return_value = mock_serial_instance
        mock_serial_instance.is_open = True

        call_count = [0]
        with patch.object(self.handler, 'list_ports', return_value=['COM1']), \
             patch.object(self.handler, 'close_port') as mock_close:
            def fake_write(data):
                call_count[0] += 1
                if call_count[0] >= 2:
                    for line in SAMPLE_RESPONSE_LINES:
                        self.handler._feed_probe(line)
            with patch.object(self.handler, 'write', side_effect=fake_write):
                result = self.handler.auto_connect(
                    settle_delay=0, retries=3, per_attempt_timeout=0.1, max_rounds=1
                )
        self.assertTrue(result)
        self.assertEqual(self.handler.current_port, 'COM1')
        mock_close.assert_not_called()

    @patch('serial_handler.serial.Serial')
    def test_status_callback_on_success(self, mock_serial):
        """Status callback receives (port, port) on success."""
        mock_serial_instance = Mock()
        mock_serial.return_value = mock_serial_instance
        mock_serial_instance.is_open = True

        statuses = []
        with patch.object(self.handler, 'list_ports', return_value=['COM1']), \
             patch.object(self.handler, 'close_port'):
            def fake_write(data):
                for line in SAMPLE_RESPONSE_LINES:
                    self.handler._feed_probe(line)
            with patch.object(self.handler, 'write', side_effect=fake_write):
                self.handler.auto_connect(
                    status_callback=statuses.append,
                    settle_delay=0, retries=1, per_attempt_timeout=0.5, max_rounds=1
                )
        self.assertIn(('COM1', 'COM1'), statuses)

    @patch('serial_handler.serial.Serial')
    def test_status_callback_no_response(self, mock_serial):
        """Status callback receives 'No response' on failure."""
        mock_serial_instance = Mock()
        mock_serial.return_value = mock_serial_instance
        mock_serial_instance.is_open = True

        statuses = []

        def fake_close_port():
            self.handler.ser = None

        with patch.object(self.handler, 'list_ports', return_value=['COM1']), \
             patch.object(self.handler, 'write'), \
             patch.object(self.handler, 'close_port', side_effect=fake_close_port):
            self.handler.auto_connect(
                status_callback=statuses.append,
                settle_delay=0, retries=1, per_attempt_timeout=0.05, max_rounds=1
            )
        self.assertIn(("No response COM1", None), statuses)
        self.assertIn(("(none)", None), statuses)

    @patch('serial_handler.serial.Serial')
    def test_infinite_loop_stops(self, mock_serial):
        """Infinite loop (max_rounds=None) stops when stop_auto_connect is called."""
        mock_serial_instance = Mock()
        mock_serial.return_value = mock_serial_instance
        mock_serial_instance.is_open = True

        with patch.object(self.handler, 'list_ports', return_value=['COM1']), \
             patch.object(self.handler, 'write'), \
             patch.object(self.handler, 'close_port'):
            threading.Timer(0.2, self.handler.stop_auto_connect).start()
            result = self.handler.auto_connect(
                settle_delay=0, retries=1, per_attempt_timeout=0.05,
                rescan_delay=0.5, max_rounds=None
            )
        self.assertFalse(result)

    @patch('serial_handler.serial.Serial')
    def test_new_port_appears_during_scan(self, mock_serial):
        """Port appearing mid-scan is detected on the next round."""
        mock_serial_instance = Mock()
        mock_serial.return_value = mock_serial_instance
        mock_serial_instance.is_open = True

        list_calls = [['COM1'], ['COM1', 'COM7']]
        with patch.object(self.handler, 'list_ports', side_effect=list_calls), \
             patch.object(self.handler, 'close_port'):
            def fake_write(data):
                if self.handler.current_port == 'COM7':
                    for line in SAMPLE_RESPONSE_LINES:
                        self.handler._feed_probe(line)
            with patch.object(self.handler, 'write', side_effect=fake_write):
                result = self.handler.auto_connect(
                    settle_delay=0, retries=1, per_attempt_timeout=0.05,
                    rescan_delay=0.01, max_rounds=None
                )
        self.assertTrue(result)
        self.assertEqual(self.handler.current_port, 'COM7')

    @patch('serial_handler.serial.Serial')
    def test_rescan_retries_old_ports(self, mock_serial):
        """After a scan round, previously-failed ports are retried."""
        mock_serial_instance = Mock()
        mock_serial.return_value = mock_serial_instance
        mock_serial_instance.is_open = True

        write_calls = [0]
        with patch.object(self.handler, 'list_ports', return_value=['COM1']), \
             patch.object(self.handler, 'close_port'):
            def fake_write(data):
                write_calls[0] += 1
                # Succeed only on the 2nd round (after tried is cleared)
                if write_calls[0] >= 2:
                    for line in SAMPLE_RESPONSE_LINES:
                        self.handler._feed_probe(line)
            with patch.object(self.handler, 'write', side_effect=fake_write):
                result = self.handler.auto_connect(
                    settle_delay=0, retries=1, per_attempt_timeout=0.05,
                    rescan_delay=0.01, max_rounds=None
                )
        self.assertTrue(result)
        self.assertGreaterEqual(write_calls[0], 2)

    def test_stop_before_start(self):
        """stop_auto_connect before auto_connect causes immediate exit."""
        self.handler.stop_auto_connect()
        with patch.object(self.handler, 'list_ports', return_value=['COM1']):
            result = self.handler.auto_connect(max_rounds=None)
        self.assertFalse(result)


if __name__ == '__main__':
    unittest.main()
