# Reactor Controller Installation Guide

This guide provides instructions for setting up both the **embedded firmware** (Arduino) and the **supervisory software** (Raspberry Pi).

## 1. Hardware Setup

1. Connect **both Arduinos** to the **Raspberry Pi** via USB.
   - **Sensor Arduino** (Arduino Mega)
   - **Pump Arduino** (Arduino Uno)
2. Ensure all sensors and heaters are correctly wired to the Mega according to the schematic.
3. Ensure the TB6600 Stepper Driver is correctly wired to the Uno (Pins 8, 9, 10).
4. Ensure the Raspberry Pi is connected to the network.

---

## 2. Firmware Installation (Arduino)

The system uses two separate firmware projects built with **PlatformIO**.

### Option A: VS Code (Recommended)
1. Install **Visual Studio Code** and the **PlatformIO IDE** extension.
2. Open the `firmware` folder to flash the **Sensor Arduino** (Mega).
3. Open the `pump_firmware` folder to flash the **Pump Arduino** (Uno).
4. Use the PlatformIO sidebar to Upload to the respective boards.

### Option B: Command Line (CLI) - Linux/Raspberry Pi
1. Install PlatformIO Core:
   ```bash
   curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py
   python3 get-platformio.py
   ```
2. Flash the Sensor Arduino:
   ```bash
   cd ~/reactor-controller/firmware
   ~/.platformio/penv/bin/pio run -t upload
   ```
3. Flash the Pump Arduino:
   ```bash
   cd ~/reactor-controller/pump_firmware
   ~/.platformio/penv/bin/pio run -t upload
   ```

---

## 3. Supervisory System Installation (Raspberry Pi)

We provide an automated deployment script to handle installation on the Raspberry Pi.

### Automated Installation
1. Copy `deploy_pi.py` to your Raspberry Pi.
2. Run the script:
   ```bash
   python3 deploy_pi.py
   ```
   *This script will:*
   * Install system dependencies (`git`, `python3-venv`).
   * Clone this repository to `~/reactor-controller`.
   * Create a virtual environment and install Python requirements.
   * Add your user to the `dialout` group for serial access.

### Running the Application
After installation, the script will provide instructions. To start the server manually:

```bash
cd ~/reactor-controller
./venv/bin/uvicorn supervisory.main:app --host 0.0.0.0 --port 8000
```

### Accessing the Interface
Open a web browser on any device on the same network and navigate to:
`http://<RASPBERRY_PI_IP>:8000`

---

## Troubleshooting

- **Serial Permission Denied**: If you get a "Permission denied" error when identifying the serial port, ensure you have rebooted or logged out/in after the installation script added you to the `dialout` group.
- **Port Not Found**: Check that the Arduino is connected. You can verify it appears in `/dev/ttyACM*` or `/dev/ttyUSB*`.
- **Blank Web Page**: Ensure you are using a modern browser. Check the JS console (F12) for errors.
