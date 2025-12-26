import urllib.request
import urllib.parse
import json
import time

BASE_URL = "http://localhost:8000"

def post_req(url):
    try:
        # Empty POST
        req = urllib.request.Request(url, method='POST')
        with urllib.request.urlopen(req) as f:
            print(f"Response: {f.status} {f.read().decode('utf-8')}")
    except Exception as e:
        print(f"Error POSTing to {url}: {e}")

def test_setpoint(zone, val, rate=0):
    print(f"Testing Setpoint Zone {zone} -> {val} (Rate: {rate})")
    url = f"{BASE_URL}/api/control/setpoint?zone={zone}&value={val}&rate={rate}"
    post_req(url)

def test_flow(val):
    print(f"Testing Flow -> {val}")
    url = f"{BASE_URL}/api/control/flow?value={val}"
    post_req(url)

def start_warmup():
    print("Setting State to WARMUP")
    url = f"{BASE_URL}/api/control/state/1"
    post_req(url)

def get_history():
    try:
        url = f"{BASE_URL}/api/history"
        with urllib.request.urlopen(url) as f:
            data = json.loads(f.read().decode('utf-8'))
            if len(data) > 0:
                # Print just a summary to avoid spam
                latest = data[-1]
                print(f"Latest Telemetry - Flow: {latest.get('sensors', {}).get('flow')} SP: {latest.get('sp', {}).get('flow')}")
            else:
                print("No history yet.")
    except Exception as e:
        print(f"Error getting history: {e}")

if __name__ == "__main__":
    print("--- Starting API Test (urllib version) ---")
    start_warmup()
    time.sleep(1)
    
    test_flow(50.0)
    time.sleep(1)
    
    test_setpoint(0, 100.0)
    time.sleep(1)
    
    test_setpoint(1, 200.0, rate=60.0)
    
    print("--- Polling History ---")
    for i in range(5):
        get_history()
        time.sleep(1)
