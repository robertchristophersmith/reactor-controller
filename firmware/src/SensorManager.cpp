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
    Serial.println("Failed: ADS MFC");
  if (!_adsPressure.begin(I2C_ADDR_ADS1115_PRESSURE))
    Serial.println("Failed: ADS Pressure");
  if (!_adsH2.begin(I2C_ADDR_ADS1115_H2))
    Serial.println("Failed: ADS H2");
}

void SensorManager::update() {
  // Read TCs
  _currentData.tempGasInternal = _tcGasInternal->readCelsius();
  _currentData.tempFeedstock = _tcFeedstock->readCelsius();
  _currentData.tempVaporizerWall = _tcVaporizerWall->readCelsius();
  _currentData.tempReactorInt1 = _tcReactorInt1->readCelsius();
  _currentData.tempReactorInt2 = _tcReactorInt2->readCelsius();
  _currentData.tempReactorExt1 = _tcReactorExt1->readCelsius();
  _currentData.tempReactorExt2 = _tcReactorExt2->readCelsius();

  // Check for errors (NaN)
  if (isnan(_currentData.tempGasInternal) ||
      isnan(_currentData.tempReactorInt1)) {
    _currentData.sensorsHealthy = false;
  } else {
    _currentData.sensorsHealthy = true;
  }

  // Read ADCs with 0.5-4.5V scaling

  // Pressure: 0.5V=0psig, 4.5V=30psig
  _currentData.pressureFeedBar = readScaled(_adsPressure, ADC_CH_PRESSURE, 0.5,
                                            4.5, 0.0, PRESSURE_MAX_PSIG);
  _currentData.pressureReactorBar = 0; // Not connected?

  // Flow: 0.5V=0sccm, 4.5V=3000sccm
  _currentData.flowRateSccm = readScaled(_adsMFC, ADC_CH_MFC_FLOW_READ, 0.5,
                                         4.5, 0.0, MFC_FLOW_MAX_SCCM);

  // H2: 0.5V=0%, 4.5V=100%
  _currentData.h2ConcentrationPpm =
      readScaled(_adsH2, ADC_CH_H2_SENSOR, 0.5, 4.5, 0.0, H2_MAX_PERCENT);
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
