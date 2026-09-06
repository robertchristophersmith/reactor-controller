# RTD Migration Implementation Plan: PWFusion SEN-30203 Quad PT100 RTD Shield

This plan details the migration of four core process temperature channels from K-type thermocouples (MAX31855/MAX31856) to **3-wire PT100 Resistance Temperature Detectors (RTDs)** using the **Playing With Fusion SEN-30203-PT100 Quad Channel MAX31865 Arduino Shield**.

The migrated zones are:
1. **Feedstock Preheater** (`t_feed_pre`)
2. **Liquid Phase Reactor** (`t_liq_reac`)
3. **Gas Phase Reactor - Interior** (`t_gas_reac_int`)
4. **Gas Phase Reactor - Exterior** (`t_gas_reac_ext`)

---

## 1. Hardware & Wiring Plan

### 1.1 Sensor Probe Termination (3-Wire PT100 to SEN-30203)

Standard 3-wire PT100 RTDs have 3 leads (typically 2 red wires and 1 white wire, or 2 common colors and 1 contrast color). The two identical wires connect to one side of the platinum sensing element, and the third wire connects to the opposite side.

Per **Figure 3 (3-Wire RTD Connection)** of the SEN-30203 datasheet:
Each of the 4 terminal blocks on the SEN-30203 has 4 spring-loaded positions labeled from top to bottom:
- `FRC+` (Force Excitation Current +)
- `RTD+` (Sense Voltage +)
- `RTD-` (Sense Voltage -)
- `FRC-` (Force Excitation Current -)

#### Wiring Table for 3-Wire PT100 Probes:
| SEN-30203 Channel | Reactor Zone | Terminal `FRC+` | Terminal `RTD+` | Terminal `RTD-` | Terminal `FRC-` |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **RTD0** | **Feedstock Preheater** | Red Lead #1 | Red Lead #2 | White Lead | *Leave Unconnected* |
| **RTD1** | **Liquid Phase Reactor** | Red Lead #1 | Red Lead #2 | White Lead | *Leave Unconnected* |
| **RTD2** | **Gas Reactor (Interior)** | Red Lead #1 | Red Lead #2 | White Lead | *Leave Unconnected* |
| **RTD3** | **Gas Reactor (Exterior)** | Red Lead #1 | Red Lead #2 | White Lead | *Leave Unconnected* |

> [!NOTE]
> For 3-wire RTDs, `FRC-` is left open. The MAX31865 performs ratiometric differential sensing between `FRC+`/`RTD+` and `RTD-`, subtracting the lead resistance measured on the duplicate `FRC+` wire to provide lead-length compensation.

---

### 1.2 Arduino Mega 2560 Interface & Pin Conflict Avoidance

> [!WARNING]
> **Pin Conflict Warning:**
> The factory default CS pins for the SEN-30203 shield are **D6 (RTD0), D7 (RTD1), D8 (RTD2), and D9 (RTD3)**.
> In this reactor system:
> - **Pin 6** is used by the **Feedstock Preheater Heater SSR (PWM)**.
> - **Pin 8** is used by the **Liquid Reactor Heater SSR (PWM)**.
> - **Pin 9** is used by the **Gas Reactor Heater SSR (PWM)**.
> 
> **Directly stacking the shield onto the Arduino Mega without modifications will create a hard electrical conflict with the SSR heater outputs.**

#### Recommended Interfacing Method: 9-Pin Auxiliary Breakout Header
The SEN-30203 features a dedicated **9-pin 0.1" (2.54mm) pitch breakout header** (`GND`, `VIN`, `SCK`, `SDO`, `SDI`, `CS3`, `CS2`, `CS1`, `CS0`). By using fly-wires from this header (or cutting shield header pins 6, 7, 8, 9 and using the CS vias), we preserve all PWM heater outputs and use dedicated CS lines.

#### Interconnection Table:

| SEN-30203 9-Pin Header | Signal Description | Arduino Mega Connection | Notes |
| :--- | :--- | :--- | :--- |
| **VIN** | Power Supply (3.3V - 5.5V) | **5V Pin** | Matches Mega 5V logic |
| **GND** | Ground | **GND Pin** | Common Ground |
| **SCK** | SPI Clock | **Pin 52** (or ICSP-3) | Hardware SPI Clock |
| **SDO** | SPI Data Out (MISO) | **Pin 50** (or ICSP-1) | Hardware SPI MISO |
| **SDI** | SPI Data In (MOSI) | **Pin 51** (or ICSP-4) | Hardware SPI MOSI (Required for MAX31865 config) |
| **CS0** | RTD0 Chip Select | **Pin 40** | Feedstock Preheater CS |
| **CS1** | RTD1 Chip Select | **Pin 42** | Liquid Reactor CS |
| **CS2** | RTD2 Chip Select | **Pin 44** | Gas Reactor Int CS |
| **CS3** | RTD3 Chip Select | **Pin 46** | Gas Reactor Ext CS |

#### Auxiliary Thermocouple Retention:
If the 2 auxiliary channels (**Feedstock Reservoir** on Pin 38 and **Electronics Housing** on Pin 36) remain on their existing thermocouple boards, they continue to share the SPI bus lines with their dedicated CS pins.

---

## 2. Software & Firmware Changes

### 2.1 Library Integration
- Integrate the official PWFusion library located in [`documents/PWFusion_MAX31865`](file:///c:/PROJECTS/reactor-controller/reactor-controller/documents/PWFusion_MAX31865) into the PlatformIO project under [`firmware/lib/PWFusion_MAX31865/`](file:///c:/PROJECTS/reactor-controller/reactor-controller/firmware/lib/).
- Add `SPI` dependency and configure PlatformIO build environment.

---

### 2.2 Firmware Updates

#### [MODIFY] [`config.h`](file:///c:/PROJECTS/reactor-controller/reactor-controller/firmware/include/config.h)
- Retain CS pin assignments (Pins 40, 42, 44, 46) and update naming conventions to reflect RTD channels:
  ```cpp
  #define PIN_SPI_CS_RTD_PREHEATER     40
  #define PIN_SPI_CS_RTD_LIQUID_REAC   42
  #define PIN_SPI_CS_RTD_GAS_REAC_INT  44
  #define PIN_SPI_CS_RTD_GAS_REAC_EXT  46
  ```

#### [MODIFY] [`SensorManager.h`](file:///c:/PROJECTS/reactor-controller/reactor-controller/firmware/include/SensorManager.h)
- Include `<PwFusion_MAX31865.h>`.
- Replace the 4 thermocouple object pointers with `MAX31865` instances:
  ```cpp
  MAX31865 _rtdPreheater;
  MAX31865 _rtdLiquidReactor;
  MAX31865 _rtdGasReactorInt;
  MAX31865 _rtdGasReactorExt;
  ```
- Retain the existing `SensorData` struct interface so downstream control loops (`HeaterController`, `main.cpp`, FSM, and Safety cutoffs) remain fully transparent and backwards compatible.

#### [MODIFY] [`SensorManager.cpp`](file:///c:/PROJECTS/reactor-controller/reactor-controller/firmware/src/SensorManager.cpp)
- **Initialization in `begin()`**:
  ```cpp
  _rtdPreheater.begin(PIN_SPI_CS_RTD_PREHEATER, RTD_3_WIRE, RTD_TYPE_PT100, SPI);
  _rtdLiquidReactor.begin(PIN_SPI_CS_RTD_LIQUID_REAC, RTD_3_WIRE, RTD_TYPE_PT100, SPI);
  _rtdGasReactorInt.begin(PIN_SPI_CS_RTD_GAS_REAC_INT, RTD_3_WIRE, RTD_TYPE_PT100, SPI);
  _rtdGasReactorExt.begin(PIN_SPI_CS_RTD_GAS_REAC_EXT, RTD_3_WIRE, RTD_TYPE_PT100, SPI);
  ```
- **High-Precision Temperature Calculation (Callendar-Van Dusen)**:
  Instead of relying on crude linear approximation (`(ADC/32) - 256`, which has up to $-1.7^\circ\text{C}$ error at $100^\circ\text{C}$ and higher error at $>300^\circ\text{C}$), compute exact temperature via the IEC 751 Callendar-Van Dusen equation using the measured resistance:
  $$R(T) = R_0(1 + A T + B T^2) \implies T = \frac{-A + \sqrt{A^2 - 4 B (1 - R / R_0)}}{2 B}$$
  where $R_0 = 100.0\,\Omega$, $A = 3.9083 \times 10^{-3}$, $B = -5.775 \times 10^{-7}$.
- **Fault Detection & Debouncing in `update()`**:
  - Sample each RTD with `.sample()`.
  - Read fault status via `.getStatus()`.
  - Map MAX31865 fault bits (`RTD_FAULT_VOLTAGE_OOR`, `RTD_FAULT_RTDIN_LOW_OPEN`, `RTD_FAULT_REFIN_LOW_OPEN`, etc.) into error fields and trigger safety states if persistent faults occur.

---

### 2.3 Supervisory & UI Compatibility

#### [MODIFY] [`orchestrator.py`](file:///c:/PROJECTS/reactor-controller/reactor-controller/supervisory/app/orchestrator.py) & [`index.html`](file:///c:/PROJECTS/reactor-controller/reactor-controller/supervisory/app/static/index.html)
- Update error byte decoding to support MAX31865 fault codes:
  - `0x04`: Voltage Out of Range / Overvoltage
  - `0x08`: RTDIn- / Force- Open
  - `0x10`: RefIn- Low / Open
  - `0x20`: RefIn- High (> 0.85 * Vbias)
  - `0x40`: Low Fault Temperature Threshold Met
  - `0x80`: High Fault Temperature Threshold Met
- Ensure UI displays accurate error descriptions when probe wiring or hardware faults occur.

---

## 3. Calibration & Validation Steps

### 3.1 Why Calibration is Different for RTDs vs Thermocouples
- **PT100 RTDs** are based on standard platinum resistance curves (IEC 751 / DIN EN 60751). Unlike thermocouples, they do not require cold junction compensation and do not suffer from cold-junction drift or thermal EMF errors.
- The **PWFusion SEN-30203 shield** includes factory-installed **0.1% precision reference resistors** ($R_{\text{ref}} = 400.0\,\Omega$), eliminating manual potentiometer tuning.

### 3.2 Required Verification & Calibration Procedures

#### Step 1: Ice Bath Zero-Point Verification ($0.0^\circ\text{C}$)
1. Prepare an insulated flask with finely crushed ice and distilled water (stirred to thermal equilibrium).
2. Insert all 4 PT100 probes into the ice bath.
3. Verify via serial output / diagnostic tool:
   - Measured resistance should be $100.00\,\Omega \pm 0.12\,\Omega$.
   - Temperature output should read $0.0^\circ\text{C} \pm 0.15^\circ\text{C}$ (Class A) or $\pm 0.3^\circ\text{C}$ (Class B).
4. If a constant offset exists due to custom probe lead lengths, record and apply a software offset $R_{\text{zero\_offset}}$.

#### Step 2: Boiling Point / Span Check ($100.0^\circ\text{C}$)
1. Place probes in boiling distilled water (or a calibrated dry block calibrator).
2. Calculate true boiling point based on local ambient atmospheric pressure.
3. Verify measured resistance is $\approx 138.51\,\Omega$ and temperature reads within $\pm 0.3^\circ\text{C}$ of the expected boiling point.

#### Step 3: Lead Resistance Compensation Check
1. Measure resistance with full cable harness connected.
2. Verify that temperature readings remain stable without drift compared to short test leads, proving that the 3rd wire cancels the wire lead resistance properly.

#### Step 4: Hardware Fault Injection Test
1. **Open Circuit Test**: Unplug the white wire (`RTD-`) from Channel 0. Verify that `getStatus()` triggers `RTD_FAULT_RTDIN_LOW_OPEN` (0x08), that the UI flags the error, and that the heater disables safely.
2. **Short Circuit Test**: Short `RTD+` to `RTD-`. Verify `RTD_FAULT_VOLTAGE_OOR` (0x04) is reported.
3. Reconnect wires and verify automatic recovery and debouncing.

---

## 4. Verification Plan

### Automated & Diagnostic Tests
1. **RTD Diagnostic Scanner**: Build an example scanner sketch in `firmware/examples/rtd_scanner.cpp` to verify all 4 channels, register outputs, resistance values, and fault registers over SPI.
2. **Firmware Compilation**: Build `[env:megaatmega2560_main]` using PlatformIO to ensure clean compilation.
3. **Supervisory Integration Tests**: Run existing test suite (`pytest c:\PROJECTS\reactor-controller\reactor-controller\supervisory\tests`) to ensure no regressions in telemetry parsing, state transitions, or UI WebSocket streams.
