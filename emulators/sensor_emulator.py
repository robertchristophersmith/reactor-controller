import asyncio
import json
import os
import time

# Simulation state
uptime = 0
state = 0  # 0=Standby, 1=Warmup, 2=Working
weight = 5.0
sp_gas = 0.0
sp_vap = 0.0
sp_reac1 = 0.0
sp_reac2 = 0.0
sp_flow = 0.0

temp_gas = 25.0
temp_vap = 25.0
temp_reac1_int = 25.0
temp_reac1_ext = 25.0
temp_reac2_int = 25.0
temp_reac2_ext = 25.0

pump_running = False
pump_dir = 0
pump_speed = 0

clients = set()

async def simulation_loop():
    global uptime, weight, temp_gas, temp_vap, temp_reac1_int, temp_reac1_ext, temp_reac2_int, temp_reac2_ext
    while True:
        await asyncio.sleep(1)
        uptime += 1
        
        # 1. Temperature Simulation
        if state in [1, 2]:  # Warmup or Working
            temp_gas += (sp_gas - temp_gas) * 0.1
            temp_vap += (sp_vap - temp_vap) * 0.1
            temp_reac1_int += (sp_reac1 - temp_reac1_int) * 0.08
            temp_reac1_ext += (sp_reac1 - temp_reac1_ext) * 0.05
            temp_reac2_int += (sp_reac2 - temp_reac2_int) * 0.08
            temp_reac2_ext += (sp_reac2 - temp_reac2_ext) * 0.05
        else:
            # Cool down to ambient (25.0)
            temp_gas += (25.0 - temp_gas) * 0.02
            temp_vap += (25.0 - temp_vap) * 0.02
            temp_reac1_int += (25.0 - temp_reac1_int) * 0.02
            temp_reac1_ext += (25.0 - temp_reac1_ext) * 0.02
            temp_reac2_int += (25.0 - temp_reac2_int) * 0.02
            temp_reac2_ext += (25.0 - temp_reac2_ext) * 0.02

        # 2. Weight Simulation
        weight_drain = (sp_flow / 100.0) * 0.01
        weight -= weight_drain
        
        if pump_running:
            pump_flow = (pump_speed / 99.0) * 0.05
            if pump_dir == 0:  # FWD
                weight += pump_flow
            else:  # REV
                weight -= pump_flow
                
        if weight < 0:
            weight = 0.0

        # 3. Broadcast Telemetry
        telemetry = {
            "uptime": uptime,
            "state": state,
            "sensors": {
                "status": 0,
                "weight": round(weight, 3)
            },
            "heaters": {
                "gas": round(100.0 if temp_gas < sp_gas else 0.0, 1),
                "vap": round(100.0 if temp_vap < sp_vap else 0.0, 1),
                "reac1": round(100.0 if temp_reac1_int < sp_reac1 else 0.0, 1),
                "reac2": round(100.0 if temp_reac2_int < sp_reac2 else 0.0, 1)
            },
            "sp": {
                "gas": sp_gas,
                "vap": sp_vap,
                "reac1": sp_reac1,
                "reac2": sp_reac2,
                "flow": sp_flow
            }
        }
        
        msg = json.dumps(telemetry) + "\n"
        for w in list(clients):
            try:
                w.write(msg.encode('utf-8'))
                await w.drain()
            except Exception:
                clients.discard(w)

async def monitor_pump():
    host = os.getenv("PUMP_EMULATOR_HOST", "localhost")
    port = int(os.getenv("PUMP_EMULATOR_PORT", "9998"))
    while True:
        try:
            reader, writer = await asyncio.open_connection(host, port)
            print(f"Sensor Emulator connected to Pump Emulator at {host}:{port}")
            while True:
                line = await reader.readline()
                if not line:
                    break
                try:
                    data = json.loads(line.decode().strip())
                    global pump_running, pump_dir, pump_speed
                    pump_running = data.get("running", False)
                    pump_dir = data.get("dir", 0)
                    pump_speed = data.get("speed", 0)
                except Exception:
                    pass
        except Exception as e:
            # print(f"Sensor Emulator failed to connect to Pump Emulator: {e}. Retrying in 2s...")
            await asyncio.sleep(2)

async def handle_client(reader, writer):
    clients.add(writer)
    print(f"Sensor Client connected")
    try:
        while True:
            line = await reader.readline()
            if not line:
                break
            decoded = line.decode('utf-8', errors='ignore').strip()
            if not decoded:
                continue
            try:
                cmd = json.loads(decoded)
                cmd_type = cmd.get("cmd")
                global state, sp_gas, sp_vap, sp_reac1, sp_reac2, sp_flow, weight
                if cmd_type == "SET_TEMP":
                    zone = cmd.get("zone")
                    val = cmd.get("val", 0.0)
                    if zone == 0:
                        sp_gas = val
                    elif zone == 1:
                        sp_vap = val
                    elif zone == 2:
                        sp_reac1 = val
                    elif zone == 3:
                        sp_reac2 = val
                elif cmd_type == "SET_STATE":
                    state = cmd.get("state", 0)
                elif cmd_type == "SET_FLOW":
                    sp_flow = cmd.get("val", 0.0)
                elif cmd_type == "TARE_LOADCELL":
                    weight = 0.0
                elif cmd_type == "CALIBRATE_LOADCELL":
                    # For simulation, setting calibrate is a no-op or aligns weight to the calibration value
                    pass
            except Exception as e:
                print(f"Error parsing command: {e}")
    except Exception as e:
        print(f"Sensor Client disconnected: {e}")
    finally:
        clients.discard(writer)

async def main():
    asyncio.create_task(simulation_loop())
    asyncio.create_task(monitor_pump())
    
    server = await asyncio.start_server(handle_client, '0.0.0.0', 9999)
    print("Sensor Emulator running on port 9999...")
    async with server:
        await server.serve_forever()

if __name__ == "__main__":
    asyncio.run(main())
