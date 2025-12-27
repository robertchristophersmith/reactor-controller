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
  // Initialize SPI TCs - Library handles SPI begin internally but good practice
  // to ensure pin modes
  pinMode(PIN_SPI_CS_TC_GAS_INTERNAL, OUTPUT);
  pinMode(PIN_SPI_CS_TC_FEEDSTOCK, OUTPUT);
  pinMode(PIN_SPI_CS_TC_VAPORIZER_WALL, OUTPUT);
  pinMode(PIN_SPI_CS_TC_REACTOR_INT_1, OUTPUT);
  pinMode(PIN_SPI_CS_TC_REACTOR_INT_2, OUTPUT);
  pinMode(PIN_SPI_CS_TC_REACTOR_EXT_1, OUTPUT);
  pinMode(PIN_SPI_CS_TC_REACTOR_EXT_2, OUTPUT);

  // Deselect all
  digitalWrite(PIN_SPI_CS_TC_GAS_INTERNAL, HIGH);
  digitalWrite(PIN_SPI_CS_TC_FEEDSTOCK, HIGH);
  digitalWrite(PIN_SPI_CS_TC_VAPORIZER_WALL, HIGH);
  digitalWrite(PIN_SPI_CS_TC_REACTOR_INT_1, HIGH);
  digitalWrite(PIN_SPI_CS_TC_REACTOR_INT_2, HIGH);
  digitalWrite(PIN_SPI_CS_TC_REACTOR_EXT_1, HIGH);
  digitalWrite(PIN_SPI_CS_TC_REACTOR_EXT_2, HIGH);

  // Initialize ADCs
  if (!_adsMFC.begin(I2C_ADDR_ADS1115_MFC))
    // Serial.println("Failed: ADS MFC");
    ;
  if (!_adsPressure.begin(I2C_ADDR_ADS1115_PRESSURE))
    // Serial.println("Failed: ADS Pressure");
    ;
  if (!_adsH2.begin(I2C_ADDR_ADS1115_H2))
    // Serial.println("Failed: ADS H2");
    ;
}

void SensorManager::update() {
  _currentData.sensorStatus = 0;

  // Read TCs and check for errors
  _currentData.tempGasInternal = _tcGasInternal->readCelsius();
  if (isnan(_currentData.tempGasInternal))
    _currentData.sensorStatus |= ERR_TC_GAS_INTERNAL;

  _currentData.tempFeedstock = _tcFeedstock->readCelsius();
  if (isnan(_currentData.tempFeedstock))
    _currentData.sensorStatus |= ERR_TC_FEEDSTOCK;

  _currentData.tempVaporizerWall = _tcVaporizerWall->readCelsius();
  if (isnan(_currentData.tempVaporizerWall))
    _currentData.sensorStatus |= ERR_TC_VAPORIZER_WALL;

  _currentData.tempReactorInt1 = _tcReactorInt1->readCelsius();
  if (isnan(_currentData.tempReactorInt1))
    _currentData.sensorStatus |= ERR_TC_REACTOR_INT_1;

  _currentData.tempReactorInt2 = _tcReactorInt2->readCelsius();
  if (isnan(_currentData.tempReactorInt2))
    _currentData.sensorStatus |= ERR_TC_REACTOR_INT_2;

  _currentData.tempReactorExt1 = _tcReactorExt1->readCelsius();
  if (isnan(_currentData.tempReactorExt1))
    _currentData.sensorStatus |= ERR_TC_REACTOR_EXT_1;

  _currentData.tempReactorExt2 = _tcReactorExt2->readCelsius();
  if (isnan(_currentData.tempReactorExt2))
    _currentData.sensorStatus |= ERR_TC_REACTOR_EXT_2;

  // Global health check based on critical sensors
  if ((_currentData.sensorStatus & ERR_TC_GAS_INTERNAL) ||
      (_currentData.sensorStatus & ERR_TC_REACTOR_INT_1)) {
    _currentData.sensorsHealthy = false;
  } else {
    _currentData.sensorsHealthy = true;
  }

  // --- Read ADCs with 0.5-4.5V scaling and Disconnect Detection ---

  // Helper to read voltage for diagnostics
  auto getVolts = [&](Adafruit_ADS1115 &ads, int ch) -> float {
    return ads.computeVolts(ads.readADC_SingleEnded(ch));
  };

  // 1. Pressure
  float pVolts = getVolts(_adsPressure, ADC_CH_PRESSURE);
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
      _currentData.pressureFeedBar = (pVolts - 0.5) * (PRESSURE_MAX_PSIG / 4.0);
  }

  _currentData.pressureReactorBar = 0; // Placeholder

  // 2. Flow (MFC)
  float fVolts = getVolts(_adsMFC, ADC_CH_MFC_FLOW_READ);
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

  // 3. H2 Sensor
  float hVolts = getVolts(_adsH2, ADC_CH_H2_SENSOR);
  if (hVolts < 0.2) {
    _currentData.sensorStatus |= ERR_H2_SENSOR;
    _currentData.h2ConcentrationPpm = 0;
  } else {
    if (hVolts <= 0.5)
      _currentData.h2ConcentrationPpm = 0.0;
    else if (hVolts >= 4.5)
      _currentData.h2ConcentrationPpm = H2_MAX_PERCENT;
    else
      _currentData.h2ConcentrationPpm = (hVolts - 0.5) * (H2_MAX_PERCENT / 4.0);
  }
}

SensorData SensorManager::getLastReadings() { return _currentData; }

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
