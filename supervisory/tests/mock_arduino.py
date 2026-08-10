import asyncio
import json
import random
import time

# Simulation Constants
HOST = '127.0.0.1'
PORT = 9999

class MockReactor:
    def __init__(self):
        self.temp_feed_res = 25.0
        self.temp_feed_pre = 25.0
        self.temp_liq_reac = 25.0
        self.temp_gas_reac_int = 25.0
        self.temp_gas_reac_ext = 25.0
        self.temp_elec_housing = 25.0

        self.sp_feed_pre = 0.0
        self.sp_liq_reac = 0.0
        self.sp_gas_reac = 0.0
        
        self.state = 0 # 0=Standby
        self.uptime = 0
        self.heater_feed_pre = 0.0
        self.heater_liq_reac = 0.0
        self.heater_gas_reac = 0.0
        
        self.start_time = time.time()

    def update(self):
        dt = 0.1 # 100ms
        self.uptime = time.time() - self.start_time
        
        # Simple thermal simulation
        # Heating
        if self.state in [1, 2]: # Warmup or Working
            self.heater_feed_pre = max(0, min(1000, (self.sp_feed_pre - self.temp_feed_pre) * 10))
            self.heater_liq_reac = max(0, min(1000, (self.sp_liq_reac - self.temp_liq_reac) * 15))
            
            # Gas average control (70% int / 30% ext)
            gas_weighted = self.temp_gas_reac_int * 0.7 + self.temp_gas_reac_ext * 0.3
            self.heater_gas_reac = max(0, min(1000, (self.sp_gas_reac - gas_weighted) * 20))
        else:
             self.heater_feed_pre = 0
             self.heater_liq_reac = 0
             self.heater_gas_reac = 0

        # Temp rise/fall (Newton's cooling law approx)
        self.temp_feed_res = 25.0  # Feedstock reservoir is not heated, stays ambient
        self.temp_feed_pre += (self.heater_feed_pre/1000.0 * 40.0 - (self.temp_feed_pre - 25.0) * 0.05) * dt
        self.temp_liq_reac += (self.heater_liq_reac/1000.0 * 30.0 - (self.temp_liq_reac - 25.0) * 0.02) * dt
        
        # Gas phase reactor internal and external dynamics
        self.temp_gas_reac_int += (self.heater_gas_reac/1000.0 * 30.0 - (self.temp_gas_reac_int - 25.0) * 0.02) * dt
        self.temp_gas_reac_ext += ((self.temp_gas_reac_int - self.temp_gas_reac_ext) * 0.1 - (self.temp_gas_reac_ext - 25.0) * 0.01) * dt
        self.temp_elec_housing += random.uniform(-0.05, 0.05)
        
        # Noise
        self.temp_feed_pre += random.uniform(-0.1, 0.1)

    def get_telemetry(self):
        return {
            "uptime": int(self.uptime),
            "state": self.state,
            "sensors": {
                "t_feed_res": round(self.temp_feed_res, 1),
                "t_feed_pre": round(self.temp_feed_pre, 1),
                "t_liq_reac": round(self.temp_liq_reac, 1),
                "t_gas_reac_int": round(self.temp_gas_reac_int, 1),
                "t_gas_reac_ext": round(self.temp_gas_reac_ext, 1),
                "t_elec_housing": round(self.temp_elec_housing, 1),
                "p_feed": 0.0,
                "p_reac": 0.0,
                "flow": 0.0,
                "h2": 0.0,
                "weight": 5.0,
                "status": 0
            },
            "heaters": {
                "feed_pre": int(self.heater_feed_pre),
                "liq_reac": int(self.heater_liq_reac),
                "gas_reac": int(self.heater_gas_reac)
            },
            "sp": {
                "feed_pre": self.sp_feed_pre,
                "liq_reac": self.sp_liq_reac,
                "gas_reac": self.sp_gas_reac
            }
        }

    def handle_command(self, cmd):
        print(f"Received: {cmd}")
        if cmd.get("cmd") == "SET_STATE":
            try:
                self.state = int(cmd.get("state"))
                print(f"MOCK: State set to {self.state}")
            except:
                print(f"MOCK ERROR: Invalid state value {cmd.get('state')}")
        elif cmd.get("cmd") == "SET_TEMP":
            z = cmd.get("zone")
            v = cmd.get("val")
            if z == 0: self.sp_feed_pre = v
            if z == 1: self.sp_liq_reac = v
            if z == 2: self.sp_gas_reac = v

async def handle_client(reader, writer):
    print("Client Connected")
    reactor = MockReactor()
    
    try:
        while True:
            # Send Telemetry (1Hz)
            reactor.update()
            if int(reactor.uptime * 10) % 10 == 0: # Approx 1Hz send
                telemetry = reactor.get_telemetry()
                writer.write((json.dumps(telemetry) + "\n").encode())
                await writer.drain()
            
            # Read Check
            try:
                data = await asyncio.wait_for(reader.readline(), timeout=0.1)
                if data:
                    try:
                        cmd = json.loads(data.decode())
                        reactor.handle_command(cmd)
                    except Exception as e:
                        print(f"JSON Error: {e}")
            except asyncio.TimeoutError:
                pass
                
            await asyncio.sleep(0.01) # fast loop
            
    except ConnectionResetError:
        print("Client Disconnected")
    except Exception as e:
        print(f"Error: {e}")

async def main():
    server = await asyncio.start_server(handle_client, HOST, PORT)
    print(f"Mock Reactor running on socket://{HOST}:{PORT}")
    async with server:
        await server.serve_forever()

if __name__ == "__main__":
    asyncio.run(main())
