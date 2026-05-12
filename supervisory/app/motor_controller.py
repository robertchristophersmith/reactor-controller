import asyncio
import logging
import serial_asyncio

logger = logging.getLogger("motor_controller")

class MotorController:
    def __init__(self, port="/dev/pump_arduino", baudrate=9600):
        self.port = port
        self.baudrate = baudrate
        self.reader = None
        self.writer = None
        
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
        try:
            self.reader, self.writer = await serial_asyncio.open_serial_connection(url=self.port, baudrate=self.baudrate)
            self.connected = True
            logger.info(f"Connected to Pump Controller at {self.port}")
        except Exception as e:
            self.connected = False
            self.error_msg = f"Connection failed: {e}"
            logger.error(self.error_msg)

    async def send_command(self, cmd: str):
        async with self._lock:
            if self.writer and self.connected:
                try:
                    self.writer.write(f"{cmd}\n".encode('utf-8'))
                    await self.writer.drain()
                except Exception as e:
                    self.connected = False
                    self.error_msg = f"Write err: {str(e)}"
                    logger.error(f"Serial write error: {e}")

    async def set_motor(self, run: bool, dir_val: int, speed: int):
        self.expected_running = run
        self.expected_dir = dir_val
        self.expected_speed = speed
        
        # Phase 2 text protocol:
        if speed != self.physical_speed:
            await self.send_command(f"<SPEED:{speed}>")
            
        dir_str = "REV" if dir_val == 1 else "FWD"
        await self.send_command(f"<DIR:{dir_str}>")
            
        if not run:
            await self.send_command("<STOP>")
        else:
            await self.send_command("<START>")

    def set_auto_mode(self, is_auto: bool):
        self.is_auto_mode = is_auto

    async def poll_loop(self):
        while True:
            await asyncio.sleep(0.5)
            # In Phase 3, we will read from self.reader to get actual status
            # For now, we mock the physical state to match expected state if connected
            if self.connected:
                self.physical_running = self.expected_running
                self.physical_dir = self.expected_dir
                self.physical_speed = self.expected_speed

motor_controller = MotorController()
