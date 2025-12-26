import asyncio
import json
import random
import time

# Simulation Constants
HOST = '127.0.0.1'
PORT = 9999

class MockReactor:
    def __init__(self):
        self.temp_gas = 25.0
        self.temp_vap = 25.0
        
        self.temp_reac_1_int = 25.0
        self.temp_reac_1_ext = 25.0
        self.temp_reac_2_int = 25.0
        self.temp_reac_2_ext = 25.0

        self.sp_gas = 0.0
        self.sp_vap = 0.0
        self.sp_reac_1 = 0.0
        self.sp_reac_2 = 0.0
        
        self.state = 0 # 0=Standby
        self.uptime = 0
        self.heater_gas = 0.0
        self.heater_vap = 0.0
        self.heater_reac_1 = 0.0
        self.heater_reac_2 = 0.0
        self.mfc_sp = 0.0
        self.mfc_flow = 0.0
        
        self.start_time = time.time()

    def update(self):
        dt = 0.1 # 100ms
        self.uptime = time.time() - self.start_time
        
        # Simple thermal simulation
        # Heating
        if self.state in [1, 2]: # Warmup or Working
            # Simple P-control simulation for heater duty
            self.heater_gas = max(0, min(1000, (self.sp_gas - self.temp_gas) * 10))
            self.heater_vap = max(0, min(1000, (self.sp_vap - self.temp_vap) * 10))
            
            # Reactor 1 Control
            pv1 = (self.temp_reac_1_int + self.temp_reac_1_ext) / 2.0
            self.heater_reac_1 = max(0, min(1000, (self.sp_reac_1 - pv1) * 30))

            # Reactor 2 Control
            pv2 = (self.temp_reac_2_int + self.temp_reac_2_ext) / 2.0
            self.heater_reac_2 = max(0, min(1000, (self.sp_reac_2 - pv2) * 30))
            
            # Flow simulation (fast response)
            # Move 10% towards setpoint per tick
            self.mfc_flow += (self.mfc_sp - self.mfc_flow) * 0.5
        else:
             self.heater_gas = 0; self.heater_vap = 0
             self.heater_reac_1 = 0; self.heater_reac_2 = 0
             self.mfc_flow = 0 # Flow cuts off

        # Temp rise/fall (Newton's cooling law approx)
        self.temp_gas += (self.heater_gas/1000.0 * 40.0 - (self.temp_gas - 25.0) * 0.05) * dt
        self.temp_vap += (self.heater_vap/1000.0 * 40.0 - (self.temp_vap - 25.0) * 0.05) * dt
        
        # Reactor 1 Dynamics (Int heats fast, Ext lags)
        self.temp_reac_1_int += (self.heater_reac_1/1000.0 * 30.0 - (self.temp_reac_1_int - 25.0) * 0.02) * dt
        self.temp_reac_1_ext += ((self.temp_reac_1_int - self.temp_reac_1_ext) * 0.1 - (self.temp_reac_1_ext - 25.0) * 0.01) * dt

        # Reactor 2 Dynamics
        self.temp_reac_2_int += (self.heater_reac_2/1000.0 * 30.0 - (self.temp_reac_2_int - 25.0) * 0.02) * dt
        self.temp_reac_2_ext += ((self.temp_reac_2_int - self.temp_reac_2_ext) * 0.1 - (self.temp_reac_2_ext - 25.0) * 0.01) * dt
        
        # Noise
        self.temp_gas += random.uniform(-0.1, 0.1)

    def get_telemetry(self):
        return {
            "uptime": int(self.uptime),
            "state": self.state,
            "sensors": {
                "t_gas": round(self.temp_gas, 1),
                "t_feed": round(self.temp_gas * 0.9, 1),
                "t_vap": round(self.temp_vap, 1),
                "t_r_i1": round(self.temp_reac_1_int, 1),
                "t_r_e1": round(self.temp_reac_1_ext, 1),
                "t_r_i2": round(self.temp_reac_2_int, 1),
                "t_r_e2": round(self.temp_reac_2_ext, 1),
                "p_feed": 0.0, # Gauge Pressure
                "p_reac": 0.0,
                "flow": round(self.mfc_flow, 1),
                "h2": 0.5 # 0% (0.5V)
            },
            "heaters": {
                "gas": int(self.heater_gas),
                "vap": int(self.heater_vap),
                "reac1": int(self.heater_reac_1),
                "reac2": int(self.heater_reac_2)
            },
            "sp": {
                "gas": self.sp_gas,
                "vap": self.sp_vap,
                "reac1": self.sp_reac_1,
                "reac2": self.sp_reac_2,
                "flow": self.mfc_sp
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
        elif cmd.get("cmd") == "SET_FLOW":
            val = cmd.get("val")
            print(f"MOCK: Setting Flow Setpoint to {val}")
            self.mfc_sp = val
        elif cmd.get("cmd") == "SET_TEMP":
            z = cmd.get("zone")
            v = cmd.get("val")
            if z == 0: self.sp_gas = v
            if z == 1: self.sp_vap = v
            if z == 2: self.sp_reac_1 = v
            if z == 3: self.sp_reac_2 = v

async def handle_client(reader, writer):
    print("Client Connected")
    reactor = MockReactor()
    
    # Send loop
    try:
        while True:
            # Check for incoming commands (non-blocking read would be ideal, but asyncio...)
            # We'll rely on reader task
            
            # Send Telemetry (1Hz)
            reactor.update()
            if int(reactor.uptime * 10) % 10 == 0: # Approx 1Hz send
                telemetry = reactor.get_telemetry()
                writer.write((json.dumps(telemetry) + "\n").encode())
                await writer.drain()
            
            # Read Check
            try:
                # Use wait_for with small timeout to poll
                data = await asyncio.wait_for(reader.readline(), timeout=0.1)
                if data:
                    try:
                        cmd = json.loads(data.decode())
                        reactor.handle_command(cmd)
                    except Exception as e:
                        print(f"JSON Error: {e}")
                else:
                    # EOF
                    # print("EOF received") # Optional
                    pass
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
