import asyncio
import json
import logging
import serial_asyncio
from typing import Callable, Optional
from .config import settings

logger = logging.getLogger("serial_link")

class SerialInterface:
    def __init__(self):
        self.reader = None
        self.writer = None
        self.running = False
        self.telemetry_callback: Optional[Callable[[dict], None]] = None

    async def connect(self):
        try:
            if settings.SERIAL_PORT.startswith("socket://"):
                # Handle socket literal for testing
                host_port = settings.SERIAL_PORT.replace("socket://", "")
                host, port = host_port.split(":")
                self.reader, self.writer = await asyncio.open_connection(host, int(port))
            else:
                self.reader, self.writer = await serial_asyncio.open_serial_connection(
                    url=settings.SERIAL_PORT, baudrate=settings.SERIAL_BAUD
                )
            logger.info(f"Connected to {settings.SERIAL_PORT}")
            self.running = True
            asyncio.create_task(self._read_loop())
        except Exception as e:
            logger.error(f"Failed to connect to serial: {e}")
            # Retry logic could go here

    async def _read_loop(self):
        while self.running:
            try:
                line = await self.reader.readline()
                if line:
                    decoded = line.decode('utf-8', errors='ignore').strip()
                    print(f"RAW SERIAL: {decoded}") # DEBUG
                    
                    try:
                        data = json.loads(decoded)
                        if "uptime" in data or "state" in data:
                             if self.telemetry_callback:
                                 await self.telemetry_callback(data)
                        elif "error" in data:
                            print(f"FIRMWARE ERROR: {data['error']}")
                            logger.error(f"FIRMWARE ERROR: {data['error']}")
                    except json.JSONDecodeError:
                        print(f"Malformed JSON: {decoded}")
                        logger.warning(f"Malformed JSON: {line}")
            except Exception as e:
                print(f"Serial read error: {e}")
                logger.error(f"Serial read error: {e}")
                await asyncio.sleep(1)

    async def send_command(self, command: dict):
        if self.writer:
            try:
                msg = json.dumps(command) + "\n"
                self.writer.write(msg.encode('utf-8'))
                await self.writer.drain()
            except Exception as e:
                logger.error(f"Write error: {e}")

    def set_telemetry_callback(self, callback):
        self.telemetry_callback = callback

serial_link = SerialInterface()
