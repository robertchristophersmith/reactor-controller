
import urllib.request
import json
import time
import sys

BASE_URL = "http://localhost:8000"

def send_setpoint(zone, value):
    url = f"{BASE_URL}/api/control/setpoint?zone={zone}&value={value}&rate=0"
    req = urllib.request.Request(url, method="POST")
    try:
        with urllib.request.urlopen(req) as response:
            return json.loads(response.read().decode())
    except urllib.error.URLError as e:
        print(f"Error sending setpoint: {e}")
        return None

def get_latest_telemetry():
    url = f"{BASE_URL}/api/history"
    try:
        with urllib.request.urlopen(url) as response:
            history = json.loads(response.read().decode())
            if history:
                return history[-1]
            return None
    except urllib.error.URLError as e:
        print(f"Error fetching history: {e}")
        return None

def test():
    print("Testing Dual-Zone Control...")
    
    # Set Zone 2 (Reactor 1) to 123.4
    print("Setting Zone 2 (Reac 1) to 123.4")
    send_setpoint(2, 123.4)
    
    # Set Zone 3 (Reactor 2) to 567.8
    print("Setting Zone 3 (Reac 2) to 567.8")
    send_setpoint(3, 567.8)
    
    # Wait for telemetry update
    print("Waiting for telemetry...")
    time.sleep(2)
    
    telemetry = get_latest_telemetry()
    if not telemetry:
        print("FAIL: No telemetry received")
        return

    sp = telemetry.get("sp", {})
    reac1_sp = sp.get("reac1")
    reac2_sp = sp.get("reac2")
    
    print(f"Telemetry SP Reac1: {reac1_sp}")
    print(f"Telemetry SP Reac2: {reac2_sp}")
    
    success = True
    if reac1_sp != 123.4:
        print("FAIL: Zone 2 Setpoint invalid")
        success = False
    
    if reac2_sp != 567.8:
        print("FAIL: Zone 3 Setpoint invalid")
        success = False
        
    if success:
        print("SUCCESS: Dual-Zone Control verified via API")
    else:
        print("FAIL: Dual-Zone Control backend check failed")

if __name__ == "__main__":
    test()
