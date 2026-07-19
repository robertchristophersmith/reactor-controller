import asyncio
import json
import os
import time

# Simulation state
uptime = 0
state = 0  # 0=Standby, 1=Warmup, 2=Working
weight = 5.0
sp_feed_pre = 0.0
sp_liq_reac = 0.0
sp_gas_reac = 0.0

temp_feed_res = 25.0
temp_feed_pre = 25.0
temp_liq_reac = 25.0
temp_gas_reac_int = 25.0
temp_gas_reac_ext = 25.0

pump_running = False
pump_dir = 0
pump_speed = 0

clients = set()

async def simulation_loop():
    global uptime, weight, temp_feed_pre, temp_liq_reac, temp_gas_reac_int, temp_gas_reac_ext
    while True:
        await asyncio.sleep(1)
        uptime += 1
        
        # 1. Temperature Simulation
        if state in [1, 2]:  # Warmup or Working
            temp_feed_pre += (sp_feed_pre - temp_feed_pre) * 0.1
            temp_liq_reac += (sp_liq_reac - temp_liq_reac) * 0.08
            temp_gas_reac_int += (sp_gas_reac - temp_gas_reac_int) * 0.08
            temp_gas_reac_ext += ((temp_gas_reac_int - temp_gas_reac_ext) * 0.1 - (temp_gas_reac_ext - 25.0) * 0.01)
        else:
            # Cool down to ambient (25.0)
            temp_feed_pre += (25.0 - temp_feed_pre) * 0.02
            temp_liq_reac += (25.0 - temp_liq_reac) * 0.02
            temp_gas_reac_int += (25.0 - temp_gas_reac_int) * 0.02
            temp_gas_reac_ext += (25.0 - temp_gas_reac_ext) * 0.02

        # 2. Weight Simulation
        if pump_running:
            pump_flow = (pump_speed / 99.0) * 0.05
            if pump_dir == 0:  # FWD
                weight += pump_flow
            else:  # REV
                weight -= pump_flow
                
        if weight < 0:
            weight = 0.0

        # 3. Broadcast Telemetry
        gas_weighted = temp_gas_reac_int * 0.7 + temp_gas_reac_ext * 0.3
        telemetry = {
            "uptime": uptime,
            "state": state,
            "sensors": {
                "status": 0,
                "weight": round(weight, 3),
                "t_feed_res": round(temp_feed_res, 1),
                "t_feed_pre": round(temp_feed_pre, 1),
                "t_liq_reac": round(temp_liq_reac, 1),
                "t_gas_reac_int": round(temp_gas_reac_int, 1),
                "t_gas_reac_ext": round(temp_gas_reac_ext, 1),
                "h2": 0.0
            },
            "heaters": {
                "feed_pre": round(100.0 if temp_feed_pre < sp_feed_pre else 0.0, 1),
                "liq_reac": round(100.0 if temp_liq_reac < sp_liq_reac else 0.0, 1),
                "gas_reac": round(100.0 if gas_weighted < sp_gas_reac else 0.0, 1)
            },
            "sp": {
                "feed_pre": sp_feed_pre,
                "liq_reac": sp_liq_reac,
                "gas_reac": sp_gas_reac
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
                global state, sp_feed_pre, sp_liq_reac, sp_gas_reac, weight
                if cmd_type == "SET_TEMP":
                    zone = cmd.get("zone")
                    val = cmd.get("val", 0.0)
                    if zone == 0:
                        sp_feed_pre = val
                    elif zone == 1:
                        sp_liq_reac = val
                    elif zone == 2:
                        sp_gas_reac = val
                elif cmd_type == "SET_STATE":
                    state = cmd.get("state", 0)
                elif cmd_type == "TARE_LOADCELL":
                    weight = 0.0
                elif cmd_type == "CALIBRATE_LOADCELL":
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
