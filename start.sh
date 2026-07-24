#!/bin/bash
# Startup script for Raspberry Pi Deployment

# Ensure we are in the script's directory
cd "$(dirname "$0")"

# Set Serial Port (Override if your Arduino is on a different port, e.g. /dev/ttyUSB0)
export SERIAL_PORT=${SERIAL_PORT:-/dev/ttyACM0}

# Activate Virtual Environment
source venv/bin/activate

# Run the Application in the background as a detached process (daemon-like)
if ! pgrep -f "uvicorn supervisory.app.main:app" > /dev/null; then
    echo "Starting Reactor Controller on $SERIAL_PORT..."
    nohup python -m uvicorn supervisory.app.main:app --host 0.0.0.0 --port 8000 > server.log 2>&1 &
    # Wait for the server to fully start
    echo "Waiting for server to spin up..."
    sleep 5
else
    echo "Server is already running!"
fi

# Launch Chromium browser in full-screen kiosk mode and detach it
echo "Launching browser..."
if command -v chromium-browser &> /dev/null; then
    nohup chromium-browser --password-store=basic --kiosk http://localhost:8000/ > /dev/null 2>&1 &
elif command -v chromium &> /dev/null; then
    nohup chromium --password-store=basic --kiosk http://localhost:8000/ > /dev/null 2>&1 &
else
    echo "Error: Chromium browser not found! Please install it with: sudo apt install chromium-browser"
fi

echo "Startup complete!"
exit 0
