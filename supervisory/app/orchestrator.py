import asyncio
from collections import deque
from .serial_interface import serial_link
from .database import SessionLocal, init_db
from .crud import create_log
import logging

logger = logging.getLogger("orchestrator")

# Buffer for live graph (last 300 points ~ 5 minutes at 1Hz)
MAX_BUFFER_SIZE = 300

class Orchestrator:
    def __init__(self):
        self.live_buffer = deque(maxlen=MAX_BUFFER_SIZE)
        self.latest_state = {}
        self.ramps = {} # {zone: {target: float, rate_per_sec: float, last_update: float}}
        self.subscribers = set() # WebSocket queues

    async def start(self):
        # Initialize DB
        init_db()
        
        # Connect Serial
        serial_link.set_telemetry_callback(self.handle_telemetry)
        await serial_link.connect()

    async def handle_telemetry(self, data: dict):
        try:
            import time
            now = time.time()

            # 1. Update Ramps
            active_ramps = list(self.ramps.keys())
            for zone in active_ramps:
                ramp = self.ramps[zone]
                # get current SP from telemetry to ensure we don't drift
                # map zone index to telemetry key
                keys = ["gas", "vap", "reac1", "reac2"]
                current_sp = data.get("sp", {}).get(keys[zone], 0.0)
                
                target = ramp["target"]
                rate = ramp["rate_per_sec"]
                
                # Calculate next step (simple linear)
                # We assume telemetry comes in at ~1Hz, but use delta time if needed
                # For simplicity, we just add rate * (now - last_update)
                dt = now - ramp["last_update"]
                
                if abs(current_sp - target) < 1.0:
                    # Done
                    del self.ramps[zone]
                    continue

                # Direction
                direction = 1 if target > current_sp else -1
                change = rate * dt * direction
                
                new_sp = current_sp + change
                
                # Clamp to target
                if (direction == 1 and new_sp > target) or (direction == -1 and new_sp < target):
                    new_sp = target
                    del self.ramps[zone]
                
                ramp["last_update"] = now
                await self.send_command_setpoint(zone, new_sp)

            # 2. Update internal state
            self.latest_state = data
            self.live_buffer.append(data)
            
            # 3. Log to Database (Sync wrapper for now, SQLite is fast enough)
            # Ideally use run_in_executor for heavy DB ops
            try:
                uptime = data.get("uptime", 0)
                state = data.get("state", 0)
                
                # Create a new session for this operation
                with SessionLocal() as db:
                    create_log(db, data, uptime, state)
                    
            except Exception as e:
                logger.error(f"DB Log Error: {e}")

            # 4. Broadcast to WebSockets
            for q in list(self.subscribers):
                try:
                    await q.put(data)
                except Exception:
                    self.subscribers.remove(q)
            print(f"Orchestrator: Processed telemetry. Buffer size: {len(self.live_buffer)}") # DEBUG
        except Exception as e:
            logger.error(f"Error in handle_telemetry: {e}")
            print(f"CRITICAL ORCHESTRATOR ERROR: {e}")

    async def send_command_setpoint(self, zone: int, value: float):
         await serial_link.send_command({"cmd": "SET_TEMP", "zone": zone, "val": value})

    async def send_flow(self, value: float):
        await serial_link.send_command({"cmd": "SET_FLOW", "val": value})

    async def send_setpoint(self, zone: int, value: float, rate_min: float = 0.0):
        import time
        if rate_min > 0:
            # Start Ramp
            self.ramps[zone] = {
                "target": value,
                "rate_per_sec": rate_min / 60.0,
                "last_update": time.time()
            }
        else:
            # Immediate
            if zone in self.ramps: del self.ramps[zone]
            await self.send_command_setpoint(zone, value)

    async def set_state(self, state: int):
        await serial_link.send_command({"cmd": "SET_STATE", "state": state})

    async def subscribe(self):
        q = asyncio.Queue()
        # Send history first
        for item in self.live_buffer:
            await q.put(item)
        self.subscribers.add(q)
        return q

    def unsubscribe(self, q):
        if q in self.subscribers:
            self.subscribers.remove(q)

orchestrator = Orchestrator()
