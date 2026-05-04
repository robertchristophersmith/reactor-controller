#!/bin/bash
# Startup script for Raspberry Pi Deployment

# Ensure we are in the script's directory
cd "$(dirname "$0")"

# Set Serial Port (Override if your Arduino is on a different port, e.g. /dev/ttyUSB0)
export SERIAL_PORT=${SERIAL_PORT:-/dev/ttyACM0}

# Activate Virtual Environment
source venv/bin/activate

# Run the Application in the background
echo "Starting Reactor Controller on $SERIAL_PORT..."
python -m uvicorn supervisory.app.main:app --host 0.0.0.0 --port 8000 &
SERVER_PID=$!

# Wait for the server to fully start
echo "Waiting for server to start..."
sleep 5

# Launch Chromium browser in full-screen kiosk mode
echo "Launching browser..."
if command -v chromium-browser &> /dev/null; then
    chromium-browser --kiosk http://localhost:8000/
elif command -v chromium &> /dev/null; then
    chromium --kiosk http://localhost:8000/
else
    echo "Error: Chromium browser not found! Please install it with: sudo apt install chromium-browser"
    # Wait indefinitely so the server stays alive in the background
    wait $SERVER_PID
fi

# If the browser is closed, kill the background server so port 8000 is freed
kill $SERVER_PID
