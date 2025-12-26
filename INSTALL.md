# Reactor Controller Installation Guide

This guide provides instructions for setting up both the **embedded firmware** (Arduino) and the **supervisory software** (Raspberry Pi).

## 1. Hardware Setup

1. Connect the **Arduino** to the **Raspberry Pi** via USB.
2. Ensure all sensors and heaters are correctly wired to the Arduino according to the schematic.
3. Ensure the Raspberry Pi is connected to the network.

---

## 2. Firmware Installation (Arduino)

The firmware is built using **PlatformIO**. You can install it using Visual Studio Code or the command line.

### Option A: VS Code (Recommended)
1. Install **Visual Studio Code**.
2. Install the **PlatformIO IDE** extension.
3. Open the `firmware` folder of this repository in VS Code.
4. Click the **PlatformIO icon** (Alien face) in the sidebar.
5. Under **Project Tasks**, select your environment (e.g., `megaatmega2560`) and click **Upload**.

### Option B: Command Line (CLI)
1. Install PlatformIO Core: `pip install platformio`
2. Navigate to the firmware directory:
   ```bash
   cd firmware
   ```
3. Build and Upload:
   ```bash
   pio run -t upload
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
