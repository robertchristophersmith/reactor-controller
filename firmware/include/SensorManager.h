#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "config.h"
#include <Adafruit_MAX31855.h>
#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>
#include <HX711.h>


struct SensorData {
  // Temperatures (Celsius)
  float tempFeedstockReservoir;
  float tempFeedstockPreheater;
  float tempLiquidReactor;
  float tempGasReactorInt;
  float tempGasReactorExt;
  float tempElectronicsHousing;

  // Exact MAX31855 Error Bytes (0x01 Open, 0x02 Short GND, 0x04 Short VCC, 0x08 NaN/comm)
  uint8_t errFeedstockReservoir;
  uint8_t errFeedstockPreheater;
  uint8_t errLiquidReactor;
  uint8_t errGasReactorInt;
  uint8_t errGasReactorExt;
  uint8_t errElectronicsHousing;

  // Analog Sensors
  float h2ConcentrationPpm;
  float weightKg;

  // Status
  // 0 = OK, Bit set = Fault (only set after 3 consecutive error reads)
  uint32_t sensorStatus;
  bool sensorsHealthy; // aggregated simple flag
};

// Error Bits
#define ERR_TC_FEEDSTOCK_RESERVOIR (1 << 0)
#define ERR_TC_FEEDSTOCK_PREHEATER (1 << 1)
#define ERR_TC_LIQUID_REACTOR (1 << 2)
#define ERR_TC_GAS_REACTOR_INT (1 << 3)
#define ERR_TC_GAS_REACTOR_EXT (1 << 4)
#define ERR_TC_ELECTRONICS_HOUSING (1 << 5)
#define ERR_H2_SENSOR (1 << 6)

class SensorManager {
public:
  SensorManager();
  void begin();
  void update();
  SensorData getLastReadings();

  void tareLoadCell();
  void calibrateLoadCell(float knownWeight);

private:
  // Thermocouple Objects
  Adafruit_MAX31855 *_tcFeedstockReservoir;
  Adafruit_MAX31855 *_tcFeedstockPreheater;
  Adafruit_MAX31855 *_tcLiquidReactor;
  Adafruit_MAX31855 *_tcGasReactorInt;
  Adafruit_MAX31855 *_tcGasReactorExt;
  Adafruit_MAX31855 *_tcElectronicsHousing;

  HX711 _hx711;

  SensorData _currentData;

  // Debouncing / Transient Filter (6 TC channels)
  float _lastValidTemp[6];
  uint8_t _errorCount[6];
  uint8_t _lastExactErr[6];
};

#endif
