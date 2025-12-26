#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "config.h"
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MAX31855.h>
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

struct SensorData {
  // Temperatures (Celsius)
  float tempGasInternal;
  float tempFeedstock;
  float tempVaporizerWall;
  float tempReactorInt1;
  float tempReactorInt2;
  float tempReactorExt1;
  float tempReactorExt2;

  // Analog Sensors
  float pressureFeedBar;
  float pressureReactorBar;
  float flowRateSccm;
  float h2ConcentrationPpm;

  // Status
  // 0 = OK, Bit set = Fault
  uint32_t sensorStatus;
  bool sensorsHealthy; // aggregated simple flag
};

// Error Bits
#define ERR_TC_GAS_INTERNAL (1 << 0)
#define ERR_TC_FEEDSTOCK (1 << 1)
#define ERR_TC_VAPORIZER_WALL (1 << 2)
#define ERR_TC_REACTOR_INT_1 (1 << 3)
#define ERR_TC_REACTOR_INT_2 (1 << 4)
#define ERR_TC_REACTOR_EXT_1 (1 << 5)
#define ERR_TC_REACTOR_EXT_2 (1 << 6)
#define ERR_P_FEED (1 << 7)
#define ERR_P_REACTOR (1 << 8)
#define ERR_MFC_FLOW (1 << 9)
#define ERR_H2_SENSOR (1 << 10)

class SensorManager {
public:
  SensorManager();
  void begin();
  void update();
  SensorData getLastReadings();

private:
  // Thermocouple Objects
  Adafruit_MAX31855 *_tcGasInternal;
  Adafruit_MAX31855 *_tcFeedstock;
  Adafruit_MAX31855 *_tcVaporizerWall;
  Adafruit_MAX31855 *_tcReactorInt1;
  Adafruit_MAX31855 *_tcReactorInt2;
  Adafruit_MAX31855 *_tcReactorExt1;
  Adafruit_MAX31855 *_tcReactorExt2;

  // ADC Objects
  Adafruit_ADS1115 _adsMFC;
  Adafruit_ADS1115 _adsPressure;
  Adafruit_ADS1115 _adsH2;

  SensorData _currentData;

  float readScaled(Adafruit_ADS1115 &ads, int channel, float vMin, float vMax,
                   float euMin, float euMax);
};

#endif
