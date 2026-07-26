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

  digitalWrite(PIN_SPI_CS_TC_FEEDSTOCK_RESERVOIR, HIGH);
  digitalWrite(PIN_SPI_CS_TC_FEEDSTOCK_PREHEATER, HIGH);
  digitalWrite(PIN_SPI_CS_TC_LIQUID_REACTOR, HIGH);
  digitalWrite(PIN_SPI_CS_TC_GAS_REACTOR_INT, HIGH);
  digitalWrite(PIN_SPI_CS_TC_GAS_REACTOR_EXT, HIGH);

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

  // Serial.println("Init: Sensors Done");
}

void SensorManager::update() {
  _currentData.sensorStatus = 0;

  // Helper to read TC and print detailed error diagnostic
  auto readTc = [&](Adafruit_MAX31855 *tc, const char *name) -> float {
    float t = tc->readCelsius();
    float internal = tc->readInternal();
    uint8_t err = tc->readError();

    if (isnan(t) || isnan(internal) || err != 0 || (t == 0.0 && internal == 0.0)) {
      if (err) {
        Serial.print("TC Error [");
        Serial.print(name);
        Serial.print("]: 0x");
        Serial.print(err, HEX);
        if (err & 0x01) Serial.print(" (OPEN CIRCUIT)");
        if (err & 0x02) Serial.print(" (SHORT TO GND)");
        if (err & 0x04) Serial.print(" (SHORT TO VCC)");
        Serial.println();
      }
      return NAN;
    }
    return t;
  };

  _currentData.tempFeedstockReservoir = readTc(_tcFeedstockReservoir, "FeedRes");
  if (isnan(_currentData.tempFeedstockReservoir))
    _currentData.sensorStatus |= ERR_TC_FEEDSTOCK_RESERVOIR;

  _currentData.tempFeedstockPreheater = readTc(_tcFeedstockPreheater, "FeedPre");
  if (isnan(_currentData.tempFeedstockPreheater))
    _currentData.sensorStatus |= ERR_TC_FEEDSTOCK_PREHEATER;

  _currentData.tempLiquidReactor = readTc(_tcLiquidReactor, "LiqReac");
  if (isnan(_currentData.tempLiquidReactor))
    _currentData.sensorStatus |= ERR_TC_LIQUID_REACTOR;

  _currentData.tempGasReactorInt = readTc(_tcGasReactorInt, "GasReacInt");
  if (isnan(_currentData.tempGasReactorInt))
    _currentData.sensorStatus |= ERR_TC_GAS_REACTOR_INT;

  _currentData.tempGasReactorExt = readTc(_tcGasReactorExt, "GasReacExt");
  if (isnan(_currentData.tempGasReactorExt))
    _currentData.sensorStatus |= ERR_TC_GAS_REACTOR_EXT;

  // Healthy if at least one connected sensor is returning valid readings
  if (isnan(_currentData.tempFeedstockReservoir) &&
      isnan(_currentData.tempFeedstockPreheater) &&
      isnan(_currentData.tempLiquidReactor) &&
      isnan(_currentData.tempGasReactorInt) &&
      isnan(_currentData.tempGasReactorExt)) {
    _currentData.sensorsHealthy = false;
  } else {
    _currentData.sensorsHealthy = true;
  }

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
