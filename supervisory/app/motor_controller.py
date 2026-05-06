import asyncio
import logging
import inspect
from pymodbus.client import AsyncModbusSerialClient
import time

logger = logging.getLogger("motor_controller")

class MotorController:
    def __init__(self, port="/dev/serial0", baudrate=9600):
        self.port = port
        self.baudrate = baudrate
        self.client = None
        self.slave_id = 1
        
        # Physical State
        self.physical_running = False
        self.physical_dir = 0
        self.physical_speed = 0
        
        # Expected State
        self.expected_running = False
        self.expected_dir = 0
        self.expected_speed = 0
        self.is_auto_mode = False
        
        # Connection Status
        self.connected = False
        self.error_msg = ""
        
        self.override_callback = None
        self._lock = asyncio.Lock()

    async def connect(self):
        self.client = AsyncModbusSerialClient(
            port=self.port,
            baudrate=self.baudrate,
            parity='N',
            stopbits=1,
            bytesize=8,
            timeout=1
        )
        await self.client.connect()
        # Take serial control initially
        await self.write_register(0x000D, 1)

    async def write_register(self, address, value):
        async with self._lock:
            if self.client and self.client.connected:
                try:
                    try:
                        await self.client.write_register(address, value, slave=self.slave_id)
                    except TypeError:
                        await self.client.write_register(address, value, unit=self.slave_id)
                except Exception as e:
                    self.connected = False
                    sig = inspect.signature(self.client.write_register)
                    self.error_msg = f"Write sig: {sig}"
                    logger.error(f"Modbus write error: {e}")

    async def read_registers(self, address, count):
        async with self._lock:
            if self.client and self.client.connected:
                try:
                    try:
                        result = await self.client.read_holding_registers(address, count, slave=self.slave_id)
                    except TypeError:
                        result = await self.client.read_holding_registers(address, count, unit=self.slave_id)
                        
                    if not result.isError():
                        return result.registers
                    else:
                        self.connected = False
                        self.error_msg = "Data error (isError=True)"
                except Exception as e:
                    self.connected = False
                    sig = inspect.signature(self.client.read_holding_registers)
                    self.error_msg = f"Read sig: {sig}"
                    logger.error(f"Modbus read error: {e}")
        return None

    async def set_motor(self, run: bool, dir_val: int, speed: int):
        self.expected_running = run
        self.expected_dir = dir_val
        self.expected_speed = speed
        
        # 0x0105: set RPM
        await self.write_register(0x0105, speed)
        
        # 0x0100: 0=forward, 1=reverse, 3=immediate stop
        if not run:
            cmd = 3
        else:
            cmd = 0 if dir_val == 0 else 1
            
        await self.write_register(0x0100, cmd)

    def set_auto_mode(self, is_auto: bool):
        self.is_auto_mode = is_auto

    async def poll_loop(self):
        while True:
            await asyncio.sleep(0.5)
            
            # Read state and dir (Register 0x0010, count 2)
            state_regs = await self.read_registers(0x0010, 2)
            if state_regs and len(state_regs) >= 2:
                self.connected = True
                self.error_msg = ""
                self.physical_running = (state_regs[0] == 1)
                self.physical_dir = state_regs[1]
            else:
                if self.connected: # Only set it once on failure
                    self.connected = False
                    self.error_msg = "Timeout or No Response"
                
            # Read speed (Register 0x0002, count 1)
            speed_reg = await self.read_registers(0x0002, 1)
            if speed_reg and len(speed_reg) >= 1:
                self.physical_speed = speed_reg[0]

            # Check for override if in auto mode
            if self.is_auto_mode:
                override = False
                if self.expected_running and not self.physical_running:
                    override = True
                elif not self.expected_running and self.physical_running:
                    override = True
                elif self.expected_running and abs(self.physical_speed - self.expected_speed) > 5:
                    override = True
                elif self.expected_running and self.physical_dir != self.expected_dir:
                    override = True
                    
                if override:
                    logger.info("Physical override detected!")
                    self.is_auto_mode = False # Suspend auto mode
                    if self.override_callback:
                        await self.override_callback()

motor_controller = MotorController()
