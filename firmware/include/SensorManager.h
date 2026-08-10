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

  // Analog Sensors
  float h2ConcentrationPpm;
  float weightKg;

  // Status
  // 0 = OK, Bit set = Fault
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

  // float readScaled(Adafruit_ADS1115 &ads, int channel, float vMin, float
  // vMax,
  //                  float euMin, float euMax);
};

#endif
