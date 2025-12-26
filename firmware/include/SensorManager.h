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
  bool sensorsHealthy;
};

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
