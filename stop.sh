#!/bin/bash
# Shutdown script for Supervisory Server

if pgrep -f "uvicorn supervisory.app.main:app" > /dev/null; then
    echo "Stopping Reactor Controller supervisory server..."
    pkill -f "uvicorn supervisory.app.main:app"
    echo "Server stopped."
else
    echo "Server is not running."
fi

# Stop Chromium browser kiosk instances
if pgrep -f "chromium" > /dev/null; then
    echo "Stopping Chromium kiosk browser..."
    pkill -f "chromium"
fi
