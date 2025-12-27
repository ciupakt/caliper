"""
Unit tests for SerialHandler
"""

import unittest
from unittest.mock import Mock, patch
import sys
import os

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from serial_handler import SerialHandler


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


if __name__ == '__main__':
    unittest.main()
