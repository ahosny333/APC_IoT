#!/usr/bin/env python3
"""
Modbus RTU Server Simulator for Testing ESP32 Modbus Master

This script creates a Modbus RTU server with the registers specified
for testing an ESP32 Modbus master implementation.
Compatible with PyModbus 3.x
"""

from pymodbus.server import StartSerialServer
from pymodbus.framer.rtu_framer import ModbusRtuFramer
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext
from pymodbus.datastore import ModbusSequentialDataBlock
from pymodbus.device import ModbusDeviceIdentification
import logging
import random
import time
from threading import Thread

# Configure logging
logging.basicConfig()
log = logging.getLogger()
log.setLevel(logging.INFO)

class ModbusRtuSimulator:
    def __init__(self, port='COM3', baudrate=9600, slave_id=1):
        self.port = port
        self.baudrate = baudrate
        self.slave_id = slave_id
        self.store = None
        self.context = None
        self.server = None
        self.running = False
        
        self.setup_datastore()
        
    def setup_datastore(self):
        """Initialize the Modbus datastore with default values"""
        # Initialize holding registers (read-write)
        hr_data = [0] * 5000  # Create 2000 holding registers, initialized to 0
        
        # Initialize input registers (read-only) with default values
        # Based on the register specifications provided
        ir_data = [0] * 200
        
        # Set initial values for the specified registers
        # Register 0: Oil pressure (0-10000 Kpa)
        ir_data[0] = 5000  # Example value
        
        # Register 1: Coolant temperature (-50 to 200 °C, signed)
        ir_data[1] = self.to_twos_complement(85, 16)  # Example value
        
        # Register 2: Oil temperature (-50 to 200 °C, signed)
        ir_data[2] = self.to_twos_complement(90, 16)  # Example value
        
        # Register 3: Fuel level (0-130 %)
        ir_data[3] = 65  # Example value
        
        # Register 4: Charge alternator voltage (0-40 V, scaled by 0.1)
        ir_data[4] = 280  # 28.0 V
        
        # Register 5: Engine Battery voltage (0-40 V, scaled by 0.1)
        ir_data[5] = 245  # 24.5 V
        
        # Register 6: Engine speed (0-6000 RPM)
        ir_data[6] = 1800  # Example value
        
        # Register 7: Generator frequency (0-70 Hz, scaled by 0.1)
        ir_data[7] = 500  # 50.0 Hz
        
        # Registers 95-123 for 8680 support (initialize with some values)
        for i in range(95, 124):
            ir_data[i] = i - 95  # Simple incrementing values
        
        # Registers 180-192 for S1/S2 load mimics
        for i in range(180, 193):
            ir_data[i] = random.randint(100, 1000)
        
        # Initialize coils and discrete inputs with zeros
        coil_data = [0x00] * 100  # Changed from False to 0x00 for PyModbus 3.x
        di_data = [0x00] * 100    # Changed from False to 0x00 for PyModbus 3.x
        
        # Create data blocks
        hr_block = ModbusSequentialDataBlock(0x00, hr_data)
        ir_block = ModbusSequentialDataBlock(0x00, ir_data)
        co_block = ModbusSequentialDataBlock(0x00, coil_data)
        di_block = ModbusSequentialDataBlock(0x00, di_data)
        
        # Create slave context
        self.store = ModbusSlaveContext(
            hr=hr_block, ir=ir_block, co=co_block, di=di_block
        )
        
        # Create server context with single slave
        self.context = ModbusServerContext(slaves={self.slave_id: self.store}, single=False)

    def ones_complement_16bit(self,value):
        """
        Calculate the 16-bit one's complement of a number.
        
        Parameters:
        value (int): The input value (should be in range 0-65535)
        
        Returns:
        int: The 16-bit one's complement of the input value
        """
        # Ensure the value is within 16-bit range
        value = value & 0xFFFF
        
        # Calculate one's complement by inverting all bits
        return (~value) & 0xFFFF
    
    def to_twos_complement(self, value, bits):
        """Convert a signed integer to two's complement format"""
        if value >= 0:
            return value
        else:
            return (1 << bits) + value
    
    def from_twos_complement(self, value, bits):
        """Convert from two's complement format to signed integer"""
        if value & (1 << (bits - 1)):
            return value - (1 << bits)
        else:
            return value
    def set_32bit_value(self, slave_context, fc, start_reg, value, signed=False):
        """
        Set a 32-bit value across two registers in big-endian format
        """
        if signed and value < 0:
            # Convert to two's complement for negative values
            value = (1 << 32) + value
        
        high_word = (value >> 16) & 0xFFFF  # Most significant 16 bits
        low_word = value & 0xFFFF           # Least significant 16 bits
        
        slave_context.setValues(fc, start_reg, [high_word])
        slave_context.setValues(fc, start_reg + 1, [low_word])
    
    def update_registers(self):
        """Thread function to periodically update register values"""
        while self.running:
            try:
                # Get the slave context
                slave_context = self.context[self.slave_id]
                offset = 1024
                
                # Update some register values with random but realistic data
                # Oil pressure (0-10000)
                new_value = random.randint(4800, 5200)
                slave_context.setValues(3, 0 + offset, [new_value])  # Changed from 3 to 4 for input registers
                
                # Coolant temperature (-50 to 200)
                temp = random.randint(-50, 0)
                slave_context.setValues(3, 1+ offset, [self.to_twos_complement(temp, 16)])
                
                # Oil temperature (-50 to 200)
                temp = random.randint(85, 100)
                slave_context.setValues(3, 2+ offset, [self.to_twos_complement(temp, 16)])
                
                # Fuel level (0-130)
                level = random.randint(60, 70)
                slave_context.setValues(3, 3+ offset, [level])
                
                # Charge alternator voltage (0-40 V, scaled by 0.1)
                voltage = random.randint(275, 285)
                slave_context.setValues(3, 4+ offset, [voltage])
                
                # Engine Battery voltage (0-40 V, scaled by 0.1)
                voltage = random.randint(240, 250)
                slave_context.setValues(3, 5+ offset, [voltage])
                
                # Engine speed (0-6000 RPM)
                speed = random.randint(1750, 1850)
                slave_context.setValues(3, 6+ offset, [speed])
                
                # main frequency (0-70 Hz, scaled by 0.1)
                freq = random.randint(495, 505)
                slave_context.setValues(3, 35+ offset, [freq])

                # Registers 36-37: Mains L1-N voltage (0-18,000 V, scaled by 0.1)
                voltage = random.randint(2180, 2220)  # 218.0-222.0 V
                self.set_32bit_value(slave_context, 3, 36 + offset , voltage)

                # Registers 38-39: Mains L2-N voltage (0-18,000 V, scaled by 0.1)
                voltage = random.randint(2180, 2220)  # 218.0-222.0 V
                self.set_32bit_value(slave_context, 3, 38 + offset, voltage)

                # Registers 40-41: Mains L3-N voltage (0-18,000 V, scaled by 0.1)
                voltage = random.randint(2180, 2220)  # 218.0-222.0 V
                self.set_32bit_value(slave_context, 3, 40 + offset, voltage)

                # Registers 42-43: Mains L1-L2 voltage (0-30,000 V, scaled by 0.1)
                voltage = random.randint(3780, 3820)  # 378.0-382.0 V
                self.set_32bit_value(slave_context, 3, 42 + offset, voltage)

                # Registers 44-45: Mains L2-L3 voltage (0-30,000 V, scaled by 0.1)
                voltage = random.randint(3780, 3820)  # 378.0-382.0 V
                self.set_32bit_value(slave_context, 3, 44 + offset, voltage)

                # Registers 46-47: Mains L3-L1 voltage (0-30,000 V, scaled by 0.1)
                voltage = random.randint(3780, 3820)  # 378.0-382.0 V
                self.set_32bit_value(slave_context, 3, 46 + offset, voltage)

                # Registers 52-53: Mains L1 current (0-99,999.9 A, scaled by 0.1)
                current = random.randint(950, 1050)  # 95.0-105.0 A
                self.set_32bit_value(slave_context, 3, 52 + offset, current)

                # Registers 54-55: Mains L2 current (0-99,999.9 A, scaled by 0.1)
                current = random.randint(950, 1050)  # 95.0-105.0 A
                self.set_32bit_value(slave_context, 3, 54 + offset, current)

                # Registers 56-57: Mains L3 current (0-99,999.9 A, scaled by 0.1)
                current = random.randint(950, 1050)  # 95.0-105.0 A
                self.set_32bit_value(slave_context, 3, 56 + offset, current)

                # Registers 58-59: Mains earth current (0-99,999.9 A, scaled by 0.1)
                current = random.randint(5, 15)  # 0.5-1.5 A
                self.set_32bit_value(slave_context, 3, 58 + offset, current)

                # Registers 60-61: Mains L1 watts (-99,999,999 to 99,999,999 W, signed)
                power = random.randint(20000, 24000)  # 20,000-24,000 W
                self.set_32bit_value(slave_context, 3, 60 + offset, power, signed=True)

                # Registers 62-63: Mains L2 watts (-99,999,999 to 99,999,999 W, signed)
                power = random.randint(20000, 24000)  # 20,000-24,000 W
                self.set_32bit_value(slave_context, 3, 62 + offset, power, signed=True)

                # Registers 64-65: Mains L3 watts (-99,999,999 to 99,999,999 W, signed)
                power = random.randint(20000, 24000)  # 20,000-24,000 W
                self.set_32bit_value(slave_context, 3, 64 + offset, power, signed=True)
                
                # # Update some of the 8680 registers (95-123)
                # for i in range(95, 124):
                #     if random.random() > 0.7:  # 30% chance to update each register
                #         current_val = slave_context.getValues(4, i, count=1)[0]
                #         new_val = max(0, min(65535, current_val + random.randint(-10, 10)))
                #         slave_context.setValues(4, i, [new_val])
                
                # # Update S1/S2 load mimic registers (180-192)
                # for i in range(180, 193):
                #     if random.random() > 0.5:  # 50% chance to update each register
                #         current_val = slave_context.getValues(4, i, count=1)[0]
                #         new_val = max(0, min(65535, current_val + random.randint(-50, 50)))
                #         slave_context.setValues(4, i, [new_val])

                # update control mode 
                if slave_context.getValues(3, 4104, count=1)[0] == 35700 and slave_context.getValues(3, 4105, count=1)[0] == self.ones_complement_16bit(35700) :
                    print("stop mode")
                    slave_context.setValues(3, 772, [0])
                if slave_context.getValues(3, 4104, count=1)[0] == 35701 and slave_context.getValues(3, 4105, count=1)[0] == self.ones_complement_16bit(35701) :
                    print("auto mode")
                    slave_context.setValues(3, 772, [1])
                if slave_context.getValues(3, 4104, count=1)[0] == 35702 and slave_context.getValues(3, 4105, count=1)[0] == self.ones_complement_16bit(35702) :
                    print("manual mode")
                    slave_context.setValues(3, 772, [2])
                
                
                # Sleep for a bit before updating again
                time.sleep(2)
                
            except Exception as e:
                log.error(f"Error in update thread: {e}")
                time.sleep(5)
    
    def start(self):
        """Start the Modbus RTU server"""
        log.info(f"Starting Modbus RTU server on {self.port}, slave ID {self.slave_id}")
        self.running = True
        
        # Start the register update thread
        update_thread = Thread(target=self.update_registers)
        update_thread.daemon = True
        update_thread.start()
        
        # Set up device identification
        identity = ModbusDeviceIdentification()
        identity.VendorName = 'ESP32 Test Server'
        identity.ProductCode = 'TEST'
        identity.VendorUrl = 'https://github.com/riptideio/pymodbus'
        identity.ProductName = 'Modbus RTU Server Simulator'
        identity.ModelName = 'ESP32 Test Model'
        identity.MajorMinorRevision = '1.0'
        
        # Start the Modbus RTU server
        self.server = StartSerialServer(
            context=self.context, 
            identity=identity,
            port=self.port,
            framer=ModbusRtuFramer,
            baudrate=self.baudrate,
            timeout=0.1,
            stopbits=1,
            bytesize=8,
            parity='N'
        )
    
    def stop(self):
        """Stop the Modbus server"""
        log.info("Stopping Modbus RTU server")
        self.running = False

def main():
    """Main function to run the Modbus RTU server"""
    # Configuration - adjust these parameters to match your setup
    serial_port = 'COM3'  # Change to your serial port (COMx on Windows, /dev/ttyUSB0 on Linux)
    baud_rate = 115200
    slave_id = 1
    
    # Create and start the Modbus simulator
    simulator = ModbusRtuSimulator(port=serial_port, baudrate=baud_rate, slave_id=slave_id)
    
    try:
        simulator.start()
    except KeyboardInterrupt:
        simulator.stop()
        log.info("Server stopped by user")
    except Exception as e:
        log.error(f"Server error: {e}")
        simulator.stop()

if __name__ == "__main__":
    main()