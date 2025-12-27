#!/bin/bash
# Startup script for Raspberry Pi Deployment

# Ensure we are in the script's directory
cd "$(dirname "$0")"

# Set Serial Port (Override if your Arduino is on a different port, e.g. /dev/ttyUSB0)
export SERIAL_PORT=${SERIAL_PORT:-/dev/ttyACM0}

# Activate Virtual Environment
source venv/bin/activate

# Run the Application
echo "Starting Reactor Controller on $SERIAL_PORT..."
python -m uvicorn supervisory.app.main:app --host 0.0.0.0 --port 8000
