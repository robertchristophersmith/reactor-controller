#include "SensorManager.h"
#include <EEPROM.h>

const int EEPROM_CALIBRATION_ADDR = 0;
const uint32_t CALIBRATION_MAGIC = 0x1A2B3C4D;

struct LoadCellCalibration {
    uint32_t magic;
    float scale;
    long offset;
};

SensorManager::SensorManager() {
  _tcFeedstockReservoir = new Adafruit_MAX31855(PIN_SPI_SCK, PIN_SPI_CS_TC_FEEDSTOCK_RESERVOIR, PIN_SPI_MISO);
  _tcFeedstockPreheater = new Adafruit_MAX31855(PIN_SPI_SCK, PIN_SPI_CS_TC_FEEDSTOCK_PREHEATER, PIN_SPI_MISO);
  _tcLiquidReactor = new Adafruit_MAX31855(PIN_SPI_SCK, PIN_SPI_CS_TC_LIQUID_REACTOR, PIN_SPI_MISO);
  _tcGasReactorInt = new Adafruit_MAX31855(PIN_SPI_SCK, PIN_SPI_CS_TC_GAS_REACTOR_INT, PIN_SPI_MISO);
  _tcGasReactorExt = new Adafruit_MAX31855(PIN_SPI_SCK, PIN_SPI_CS_TC_GAS_REACTOR_EXT, PIN_SPI_MISO);
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

  // Initialize Chip Selects
  pinMode(PIN_SPI_CS_TC_FEEDSTOCK_RESERVOIR, OUTPUT);
  pinMode(PIN_SPI_CS_TC_FEEDSTOCK_PREHEATER, OUTPUT);
  pinMode(PIN_SPI_CS_TC_LIQUID_REACTOR, OUTPUT);
  pinMode(PIN_SPI_CS_TC_GAS_REACTOR_INT, OUTPUT);
  pinMode(PIN_SPI_CS_TC_GAS_REACTOR_EXT, OUTPUT);
  pinMode(PIN_SPI_CS_TC_ELECTRONICS_HOUSING, OUTPUT);

  digitalWrite(PIN_SPI_CS_TC_FEEDSTOCK_RESERVOIR, HIGH);
  digitalWrite(PIN_SPI_CS_TC_FEEDSTOCK_PREHEATER, HIGH);
  digitalWrite(PIN_SPI_CS_TC_LIQUID_REACTOR, HIGH);
  digitalWrite(PIN_SPI_CS_TC_GAS_REACTOR_INT, HIGH);
  digitalWrite(PIN_SPI_CS_TC_GAS_REACTOR_EXT, HIGH);
  digitalWrite(PIN_SPI_CS_TC_ELECTRONICS_HOUSING, HIGH);

  // Initialize Hardware SPI before TC instances begin
  SPI.begin();
  // Slow down SPI to ~500kHz (16MHz / 32) for stability
  SPI.setClockDivider(SPI_CLOCK_DIV32);

  // Initialize TC instances
  if (!_tcFeedstockReservoir->begin())
    Serial.println("TC Feedstock Res init failed");
  if (!_tcFeedstockPreheater->begin())
    Serial.println("TC Feedstock Pre init failed");
  if (!_tcLiquidReactor->begin())
    Serial.println("TC Liquid Reac init failed");
  if (!_tcGasReactorInt->begin())
    Serial.println("TC Gas Reac Int init failed");
  if (!_tcGasReactorExt->begin())
    Serial.println("TC Gas Reac Ext init failed");
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

void SensorManager::update() {
  _currentData.sensorStatus = 0;

  // Helper with 3-read transient debouncer and exact error byte extraction
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

    if (_errorCount[idx] < 3) {
      // Transient error (1 or 2 reads): hold previous valid temp, do NOT set error bit yet
      return _lastValidTemp[idx];
    } else {
      // Persistent error (3+ reads): set error bit and return previous valid temp for safe PID control
      _currentData.sensorStatus |= errBit;
      return _lastValidTemp[idx];
    }
  };

  _currentData.tempFeedstockReservoir = readTcDebounced(_tcFeedstockReservoir, "FeedRes", 0, ERR_TC_FEEDSTOCK_RESERVOIR, _currentData.errFeedstockReservoir);
  _currentData.tempFeedstockPreheater = readTcDebounced(_tcFeedstockPreheater, "FeedPre", 1, ERR_TC_FEEDSTOCK_PREHEATER, _currentData.errFeedstockPreheater);
  _currentData.tempLiquidReactor = readTcDebounced(_tcLiquidReactor, "LiqReac", 2, ERR_TC_LIQUID_REACTOR, _currentData.errLiquidReactor);
  _currentData.tempGasReactorInt = readTcDebounced(_tcGasReactorInt, "GasReacInt", 3, ERR_TC_GAS_REACTOR_INT, _currentData.errGasReactorInt);
  _currentData.tempGasReactorExt = readTcDebounced(_tcGasReactorExt, "GasReacExt", 4, ERR_TC_GAS_REACTOR_EXT, _currentData.errGasReactorExt);
  _currentData.tempElectronicsHousing = readTcDebounced(_tcElectronicsHousing, "ElecHousing", 5, ERR_TC_ELECTRONICS_HOUSING, _currentData.errElectronicsHousing);

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

/*
float SensorManager::readScaled(Adafruit_ADS1115 &ads, int channel, float vMin,
                                float vMax, float euMin, float euMax) {
  int16_t adc = ads.readADC_SingleEnded(channel);
  float voltage = ads.computeVolts(adc);

  if (voltage <= vMin)
    return euMin;
  if (voltage >= vMax)
    return euMax;

  return euMin + (voltage - vMin) * (euMax - euMin) / (vMax - vMin);
}
*/
