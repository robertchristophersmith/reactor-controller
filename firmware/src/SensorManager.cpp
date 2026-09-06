#include "SensorManager.h"
#include <EEPROM.h>
#include <math.h>

const int EEPROM_CALIBRATION_ADDR = 0;
const uint32_t CALIBRATION_MAGIC = 0x1A2B3C4D;

struct LoadCellCalibration {
    uint32_t magic;
    float scale;
    long offset;
};

SensorManager::SensorManager() {
  _tcFeedstockReservoir = new Adafruit_MAX31855(PIN_SPI_SCK, PIN_SPI_CS_TC_FEEDSTOCK_RESERVOIR, PIN_SPI_MISO);
  _tcElectronicsHousing = new Adafruit_MAX31855(PIN_SPI_SCK, PIN_SPI_CS_TC_ELECTRONICS_HOUSING, PIN_SPI_MISO);

  for (int i = 0; i < 6; i++) {
    _lastValidTemp[i] = 25.0f; // Default room ambient fallback
    _errorCount[i] = 0;
    _lastExactErr[i] = 0;
  }
}

void SensorManager::begin() {
  // ATmega2560 Hardware SS (Pin 53) MUST be OUTPUT HIGH for SPI Master Mode
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);

  // Initialize Thermocouple Chip Selects
  pinMode(PIN_SPI_CS_TC_FEEDSTOCK_RESERVOIR, OUTPUT);
  pinMode(PIN_SPI_CS_TC_ELECTRONICS_HOUSING, OUTPUT);
  digitalWrite(PIN_SPI_CS_TC_FEEDSTOCK_RESERVOIR, HIGH);
  digitalWrite(PIN_SPI_CS_TC_ELECTRONICS_HOUSING, HIGH);

  // Initialize RTD Chip Selects (SEN-30203 Quad Shield)
  pinMode(PIN_SPI_CS_RTD_FEEDSTOCK_PREHEATER, OUTPUT);
  pinMode(PIN_SPI_CS_RTD_LIQUID_REACTOR, OUTPUT);
  pinMode(PIN_SPI_CS_RTD_GAS_REACTOR_INT, OUTPUT);
  pinMode(PIN_SPI_CS_RTD_GAS_REACTOR_EXT, OUTPUT);
  digitalWrite(PIN_SPI_CS_RTD_FEEDSTOCK_PREHEATER, HIGH);
  digitalWrite(PIN_SPI_CS_RTD_LIQUID_REACTOR, HIGH);
  digitalWrite(PIN_SPI_CS_RTD_GAS_REACTOR_INT, HIGH);
  digitalWrite(PIN_SPI_CS_RTD_GAS_REACTOR_EXT, HIGH);

  // Initialize Hardware SPI for MAX31865 RTD shield
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV16);

  // Initialize RTD channels (3-wire PT100)
  _rtdPreheater.begin(PIN_SPI_CS_RTD_FEEDSTOCK_PREHEATER, RTD_3_WIRE, RTD_TYPE_PT100, SPI);
  _rtdLiquidReactor.begin(PIN_SPI_CS_RTD_LIQUID_REACTOR, RTD_3_WIRE, RTD_TYPE_PT100, SPI);
  _rtdGasReactorInt.begin(PIN_SPI_CS_RTD_GAS_REACTOR_INT, RTD_3_WIRE, RTD_TYPE_PT100, SPI);
  _rtdGasReactorExt.begin(PIN_SPI_CS_RTD_GAS_REACTOR_EXT, RTD_3_WIRE, RTD_TYPE_PT100, SPI);

  // Initialize Software SPI TC instances (auxiliary channels)
  if (!_tcFeedstockReservoir->begin())
    Serial.println("TC Feedstock Res init failed");
  if (!_tcElectronicsHousing->begin())
    Serial.println("TC Electronics Housing init failed");

  // Initialize Analog H2 Sensor Pin
  pinMode(PIN_H2_SENSOR, INPUT);

  // Initialize HX711
  Serial.println("Init: HX711 Load Cell...");
  _hx711.begin(PIN_HX711_DT, PIN_HX711_SCK);
  
  LoadCellCalibration cal;
  EEPROM.get(EEPROM_CALIBRATION_ADDR, cal);

  if (cal.magic == CALIBRATION_MAGIC) {
    Serial.println("Loading Load Cell calibration from EEPROM");
    _hx711.set_scale(cal.scale);
    _hx711.set_offset(cal.offset);
  } else {
    Serial.println("No valid calibration found in EEPROM. Using defaults.");
    _hx711.set_scale(1.f); // Default scale
    _hx711.tare();         // Auto-tare on startup
  }
}

float SensorManager::calculateCvdTemperature(float rOhms) {
  if (isnan(rOhms) || rOhms <= 0.0f) {
    return NAN;
  }
  // Callendar-Van Dusen equation (IEC 751 / DIN EN 60751):
  // R(T) = R0 * (1 + A * T + B * T^2)
  // T = (-A + sqrt(A^2 - 4 * B * (1 - R / R0))) / (2 * B)
  const float R0 = 100.0f;
  const float A = 3.9083e-3f;
  const float B = -5.775e-7f;

  float discriminant = (A * A) - (4.0f * B * (1.0f - (rOhms / R0)));
  if (discriminant < 0.0f) {
    return NAN;
  }
  return (-A + sqrt(discriminant)) / (2.0f * B);
}

void SensorManager::update() {
  _currentData.sensorStatus = 0;

  // Helper with 5-read transient debouncer for MAX31855 thermocouples
  auto readTcDebounced = [&](Adafruit_MAX31855 *tc, const char *name, int idx, uint32_t errBit, uint8_t &exactErrOut) -> float {
    float t = tc->readCelsius();
    float internal = tc->readInternal();
    uint8_t err = tc->readError();

    bool isError = isnan(t) || isnan(internal) || err != 0 || (t == 0.0 && internal == 0.0);

    if (!isError) {
      // Valid reading: clear error counter and save last valid temp
      _errorCount[idx] = 0;
      _lastExactErr[idx] = 0;
      _lastValidTemp[idx] = t;
      exactErrOut = 0;
      return t;
    }

    // Error detected
    _errorCount[idx]++;
    exactErrOut = (err != 0) ? err : 0x08; // 0x08 = NaN / Comm Fault
    _lastExactErr[idx] = exactErrOut;

    if (_errorCount[idx] < 6) {
      // Transient error (1 to 5 reads): hold previous valid temp, do NOT set error bit yet
      return _lastValidTemp[idx];
    } else {
      // Persistent error (6th+ read): set error bit and return previous valid temp for safe PID control
      _currentData.sensorStatus |= errBit;
      return _lastValidTemp[idx];
    }
  };

  // Helper with 5-read transient debouncer for MAX31865 RTDs
  auto readRtdDebounced = [&](MAX31865 &rtd, const char *name, int idx, uint32_t errBit, uint8_t &exactErrOut) -> float {
    rtd.sample();
    uint8_t status = rtd.getStatus();
    float r = rtd.getResistance();
    float t = calculateCvdTemperature(r);

    bool isError = (status != 0) || isnan(t) || (r <= 0.0f);

    if (!isError) {
      // Valid reading: clear error counter and save last valid temp
      _errorCount[idx] = 0;
      _lastExactErr[idx] = 0;
      _lastValidTemp[idx] = t;
      exactErrOut = 0;
      return t;
    }

    // Error detected
    _errorCount[idx]++;
    exactErrOut = (status != 0) ? status : 0x08; // 0x08 = Comm Fault / NaN
    _lastExactErr[idx] = exactErrOut;

    if (_errorCount[idx] < 6) {
      // Transient error (1 to 5 reads): hold previous valid temp
      return _lastValidTemp[idx];
    } else {
      // Persistent error (6th+ read): set error bit and return previous valid temp for safe PID control
      _currentData.sensorStatus |= errBit;
      return _lastValidTemp[idx];
    }
  };

  // Read Auxiliary Thermocouples
  _currentData.tempFeedstockReservoir = readTcDebounced(_tcFeedstockReservoir, "FeedRes", 0, ERR_TC_FEEDSTOCK_RESERVOIR, _currentData.errFeedstockReservoir);
  _currentData.tempElectronicsHousing = readTcDebounced(_tcElectronicsHousing, "ElecHousing", 5, ERR_TC_ELECTRONICS_HOUSING, _currentData.errElectronicsHousing);

  // Read Core Process RTDs (PWFusion SEN-30203 Quad MAX31865)
  _currentData.tempFeedstockPreheater = readRtdDebounced(_rtdPreheater, "FeedPre", 1, ERR_RTD_FEEDSTOCK_PREHEATER, _currentData.errFeedstockPreheater);
  _currentData.tempLiquidReactor = readRtdDebounced(_rtdLiquidReactor, "LiqReac", 2, ERR_RTD_LIQUID_REACTOR, _currentData.errLiquidReactor);
  _currentData.tempGasReactorInt = readRtdDebounced(_rtdGasReactorInt, "GasReacInt", 3, ERR_RTD_GAS_REACTOR_INT, _currentData.errGasReactorInt);
  _currentData.tempGasReactorExt = readRtdDebounced(_rtdGasReactorExt, "GasReacExt", 4, ERR_RTD_GAS_REACTOR_EXT, _currentData.errGasReactorExt);

  _currentData.sensorsHealthy = (_currentData.sensorStatus == 0);

  // Read Analog H2 Sensor (MQ-8)
  int rawH2 = analogRead(PIN_H2_SENSOR);
  float voltageH2 = rawH2 * (5.0f / 1023.0f);
  _currentData.h2ConcentrationPpm = voltageH2 * 1000.0f; // Scale 0-5V to 0-5000 ppm estimation

  // Read Load Cell
  if (_hx711.is_ready()) {
    // Read a single raw value scaled by current calibration factor
    float newWeight = _hx711.get_units(1);
    
    // Apply an Exponential Moving Average (EMA) filter to smooth the noise
    // 20% new reading, 80% old reading.
    if (_currentData.weightKg == 0.0) {
       // Initialize on first read
       _currentData.weightKg = newWeight;
    } else {
       _currentData.weightKg = (0.2 * newWeight) + (0.8 * _currentData.weightKg);
    }
  }
}

SensorData SensorManager::getLastReadings() { return _currentData; }

void SensorManager::tareLoadCell() {
  // tare() internally waits until ready and averages 10 readings
  _hx711.tare(10); 
  _currentData.weightKg = 0.0; // Reset the EMA filter instantly

  // Save new offset to EEPROM
  LoadCellCalibration cal;
  EEPROM.get(EEPROM_CALIBRATION_ADDR, cal);
  cal.magic = CALIBRATION_MAGIC;
  cal.offset = _hx711.get_offset();
  cal.scale = _hx711.get_scale(); 
  EEPROM.put(EEPROM_CALIBRATION_ADDR, cal);
}

void SensorManager::calibrateLoadCell(float knownWeight) {
  if (knownWeight != 0) {
    // get_value(10) internally waits until ready and averages 10 readings minus offset
    long reading = _hx711.get_value(10); 
    
    // Safeguard: If the reading is extremely small, it means the user forgot to put 
    // a weight on the scale before clicking calibrate. This prevents a scale factor 
    // near 0, which would multiply noise into thousands of Kg!
    if (abs(reading) > 500) {
      float scale = (float)reading / knownWeight;
      _hx711.set_scale(scale);
      _currentData.weightKg = knownWeight; // Snap EMA to known weight

      // Save new scale to EEPROM
      LoadCellCalibration cal;
      EEPROM.get(EEPROM_CALIBRATION_ADDR, cal);
      cal.magic = CALIBRATION_MAGIC;
      cal.scale = scale;
      cal.offset = _hx711.get_offset();
      EEPROM.put(EEPROM_CALIBRATION_ADDR, cal);
    }
  }
}
