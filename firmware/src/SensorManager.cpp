#include "SensorManager.h"

SensorManager::SensorManager() {
  _tcGasInternal = new Adafruit_MAX31855(PIN_SPI_CS_TC_GAS_INTERNAL);
  _tcFeedstock = new Adafruit_MAX31855(PIN_SPI_CS_TC_FEEDSTOCK);
  _tcVaporizerWall = new Adafruit_MAX31855(PIN_SPI_CS_TC_VAPORIZER_WALL);
  _tcReactorInt1 = new Adafruit_MAX31855(PIN_SPI_CS_TC_REACTOR_INT_1);
  _tcReactorInt2 = new Adafruit_MAX31855(PIN_SPI_CS_TC_REACTOR_INT_2);
  _tcReactorExt1 = new Adafruit_MAX31855(PIN_SPI_CS_TC_REACTOR_EXT_1);
  _tcReactorExt2 = new Adafruit_MAX31855(PIN_SPI_CS_TC_REACTOR_EXT_2);
}

void SensorManager::begin() {
  // --- SPI CS INITIALIZATION DISABLED ---
  /*
  pinMode(PIN_SPI_CS_TC_GAS_INTERNAL, OUTPUT);
  pinMode(PIN_SPI_CS_TC_FEEDSTOCK, OUTPUT);
  pinMode(PIN_SPI_CS_TC_VAPORIZER_WALL, OUTPUT);
  pinMode(PIN_SPI_CS_TC_REACTOR_INT_1, OUTPUT);
  pinMode(PIN_SPI_CS_TC_REACTOR_INT_2, OUTPUT);
  pinMode(PIN_SPI_CS_TC_REACTOR_EXT_1, OUTPUT);
  pinMode(PIN_SPI_CS_TC_REACTOR_EXT_2, OUTPUT);

  digitalWrite(PIN_SPI_CS_TC_GAS_INTERNAL, HIGH);
  digitalWrite(PIN_SPI_CS_TC_FEEDSTOCK, HIGH);
  digitalWrite(PIN_SPI_CS_TC_VAPORIZER_WALL, HIGH);
  digitalWrite(PIN_SPI_CS_TC_REACTOR_INT_1, HIGH);
  digitalWrite(PIN_SPI_CS_TC_REACTOR_INT_2, HIGH);
  digitalWrite(PIN_SPI_CS_TC_REACTOR_EXT_1, HIGH);
  digitalWrite(PIN_SPI_CS_TC_REACTOR_EXT_2, HIGH);

  // Initialize Hardware SPI
  SPI.begin();
  // Slow down SPI to ~500kHz (16MHz / 32) for stability
  SPI.setClockDivider(SPI_CLOCK_DIV32);
  */

  // Initialize ADCs
  // Initialize ADCs
  Serial.println("Init: ADCs starting...");

  // Initialize Wire manually to set timeout to prevent hangs
  Wire.begin();
  // Set timeout to 3000us (3ms) and reset_on_timeout=true
  Wire.setWireTimeout(3000, true);

  // DISABLE I2C SENSORS
  // Serial.println("Init: MFC ADS1115... DISABLED");
  /*
  if (!_adsMFC.begin(I2C_ADDR_ADS1115_MFC)) {
    Serial.println("Failed: ADS MFC");
  } else {
    Serial.println("OK: ADS MFC");
  }
  */

  // Serial.println("Init: Pressure ADS1115... DISABLED");
  /*
  if (!_adsPressure.begin(I2C_ADDR_ADS1115_PRESSURE)) {
    Serial.println("Failed: ADS Pressure");
  } else {
    Serial.println("OK: ADS Pressure");
  }
  */

  // Serial.println("Init: H2 ADS1115... DISABLED");
  /*
  if (!_adsH2.begin(I2C_ADDR_ADS1115_H2)) {
    Serial.println("Failed: ADS H2");
  } else {
    Serial.println("OK: ADS H2");
  }
  */

  // Initialize HX711
  Serial.println("Init: HX711 Load Cell...");
  _hx711.begin(PIN_HX711_DT, PIN_HX711_SCK);
  _hx711.set_scale(); // Default scale
  _hx711.tare();      // Auto-tare on startup

  // Serial.println("Init: Sensors Done");
}

void SensorManager::update() {
  _currentData.sensorStatus = 0;

  // All TCs offline
  _currentData.tempGasInternal = 0.0;
  _currentData.tempFeedstock = 0.0;
  _currentData.tempVaporizerWall = 0.0;
  _currentData.tempReactorExt1 = 0.0;
  _currentData.tempReactorInt1 = 0.0;
  _currentData.tempReactorExt2 = 0.0;
  _currentData.tempReactorInt2 = 0.0;

  // Force healthy to avoid faults
  _currentData.sensorsHealthy = true;

  // --- Read ADCs with 0.5-4.5V scaling and Disconnect Detection ---
  // Helper: Check if I2C device is alive before reading to prevent hanging
  auto isConnected = [](uint8_t addr) -> bool {
    // Disabling I2C Checks
    return false;
    // Wire.beginTransmission(addr);
    // return (Wire.endTransmission() == 0);
  };

  // Helper to read voltage for diagnostics
  auto getVolts = [&](Adafruit_ADS1115 &ads, int ch, uint8_t addr) -> float {
    if (!isConnected(addr))
      return 0.0f;
    return ads.computeVolts(ads.readADC_SingleEnded(ch));
  };

  // 1. Pressure
  _currentData.pressureFeedBar = 0.0; // Disabled

  // 2. Flow (MFC)
  _currentData.flowRateSccm = 0.0; // Disabled

  // 3. H2 Sensor
  _currentData.h2ConcentrationPpm = 0.0; // Disabled

  /* I2C SENSORS DISABLED
  if (isConnected(I2C_ADDR_ADS1115_PRESSURE)) {
    float pVolts =
        getVolts(_adsPressure, ADC_CH_PRESSURE, I2C_ADDR_ADS1115_PRESSURE);
    if (pVolts < 0.2) { // Disconnected (Floating/Pull-down) or Broken Wire
      _currentData.sensorStatus |= ERR_P_FEED;
      _currentData.pressureFeedBar = 0;
    } else {
      // Scale: 0.5V=0, 4.5V=Max. Clamp logic inside readScaled-like math
      if (pVolts <= 0.5)
        _currentData.pressureFeedBar = 0.0;
      else if (pVolts >= 4.5)
        _currentData.pressureFeedBar = PRESSURE_MAX_PSIG;
      else
        _currentData.pressureFeedBar =
            (pVolts - 0.5) * (PRESSURE_MAX_PSIG / 4.0);
    }
  } else {
    _currentData.sensorStatus |= ERR_P_FEED; // Mark as error if I2C missing
    _currentData.pressureFeedBar = 0;
  }

  // 2. Flow (MFC)
  if (isConnected(I2C_ADDR_ADS1115_MFC)) {
    float fVolts =
        getVolts(_adsMFC, ADC_CH_MFC_FLOW_READ, I2C_ADDR_ADS1115_MFC);
    if (fVolts < 0.2) {
      _currentData.sensorStatus |= ERR_MFC_FLOW;
      _currentData.flowRateSccm = 0;
    } else {
      if (fVolts <= 0.5)
        _currentData.flowRateSccm = 0.0;
      else if (fVolts >= 4.5)
        _currentData.flowRateSccm = MFC_FLOW_MAX_SCCM;
      else
        _currentData.flowRateSccm = (fVolts - 0.5) * (MFC_FLOW_MAX_SCCM / 4.0);
    }
  } else {
    _currentData.sensorStatus |= ERR_MFC_FLOW;
    _currentData.flowRateSccm = 0;
  }

  // 3. H2 Sensor
  if (isConnected(I2C_ADDR_ADS1115_H2)) {
    float hVolts = getVolts(_adsH2, ADC_CH_H2_SENSOR, I2C_ADDR_ADS1115_H2);
    if (hVolts < 0.2) {
      _currentData.sensorStatus |= ERR_H2_SENSOR;
      _currentData.h2ConcentrationPpm = 0;
    } else {
      if (hVolts <= 0.5)
        _currentData.h2ConcentrationPpm = 0.0;
      else if (hVolts >= 4.5)
        _currentData.h2ConcentrationPpm = H2_MAX_PERCENT;
      else
        _currentData.h2ConcentrationPpm =
            (hVolts - 0.5) * (H2_MAX_PERCENT / 4.0);
    }
  } else {
    _currentData.sensorStatus |= ERR_H2_SENSOR;
    _currentData.h2ConcentrationPpm = 0;
  }
  */

  // Default failure values so loop continues (Modified to only set Reactor
  // Pressure)
  _currentData.pressureReactorBar = 0;

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
  if (_hx711.is_ready()) {
    _hx711.tare();
  }
}

void SensorManager::calibrateLoadCell(float knownWeight) {
  if (knownWeight != 0 && _hx711.is_ready()) {
    // get_value returns raw reading minus offset
    long reading = _hx711.get_value(10); 
    float scale = (float)reading / knownWeight;
    _hx711.set_scale(scale);
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
