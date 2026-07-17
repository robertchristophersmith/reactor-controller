import asyncio
import logging
import serial_asyncio
from .config import settings

logger = logging.getLogger("motor_controller")

class MotorController:
    def __init__(self, port=None, baudrate=None):
        self.port = port if port is not None else settings.PUMP_PORT
        self.baudrate = baudrate if baudrate is not None else settings.PUMP_BAUD
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
            if self.port.startswith("socket://"):
                host_port = self.port.replace("socket://", "")
                host, port = host_port.split(":")
                self.reader, self.writer = await asyncio.open_connection(host, int(port))
            else:
                self.reader, self.writer = await serial_asyncio.open_serial_connection(url=self.port, baudrate=self.baudrate)
            self.connected = True
            logger.info(f"Connected to Pump Controller at {self.port}")
            # Start read loop
            asyncio.create_task(self._read_loop())
        except Exception as e:
            self.connected = False
            self.error_msg = f"Connection failed: {e}"
            logger.error(self.error_msg)

    async def _read_loop(self):
        while self.connected:
            try:
                line = await self.reader.readline()
                if not line:
                    self.connected = False
                    self.error_msg = "Connection lost"
                    break
                decoded = line.decode('utf-8', errors='ignore').strip()
                if decoded.startswith("{"):
                    continue
                logger.debug(f"PUMP SERIAL: {decoded}")
                
                # Check for OK responses to update physical state
                if decoded == "OK:START":
                    self.physical_running = True
                elif decoded == "OK:STOP":
                    self.physical_running = False
                elif decoded == "OK:DIR_FWD":
                    self.physical_dir = 0
                elif decoded == "OK:DIR_REV":
                    self.physical_dir = 1
                elif decoded.startswith("OK:SPEED_"):
                    try:
                        self.physical_speed = int(decoded.split("_")[1])
                    except Exception:
                        pass
            except Exception as e:
                logger.error(f"Pump serial read error: {e}")
                await asyncio.sleep(1)

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
            # In Phase 3 / Dev emulator mode, physical state is updated by _read_loop.
            # To ensure local testing works cleanly, if connection is lost, reset state:
            if not self.connected:
                self.physical_running = False
                self.physical_speed = 0

motor_controller = MotorController()
