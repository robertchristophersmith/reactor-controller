#ifndef HEATER_CONTROLLER_H
#define HEATER_CONTROLLER_H

#include "config.h"
#include <Arduino.h>
#include <PID_v1.h>

// Time Proportional Window Size (ms)
#define WINDOW_SIZE 1000

class HeaterController {
public:
  HeaterController();
  void begin();

  void setSetpoints(float spPreheater, float spLiquid, float spGas);
  void update(float tempPreheater, float tempLiquid, float tempGas);
  void setEnabled(bool enabled);

  // Telemetry getters
  float getOutputPreheater() { return _outPreheater; }
  float getOutputLiquid() { return _outLiquid; }
  float getOutputGas() { return _outGas; }

  float getSetpointPreheater() { return _spPreheater; }
  float getSetpointLiquid() { return _spLiquid; }
  float getSetpointGas() { return _spGas; }

private:
  bool _enabled;
  unsigned long _windowStartTime;

  // PID Variables (Double required by Library)
  double _spPreheater, _inPreheater, _outPreheater;
  double _spLiquid, _inLiquid, _outLiquid;
  double _spGas, _inGas, _outGas;

  // PID Objects
  PID *_pidPreheater;
  PID *_pidLiquid;
  PID *_pidGas;

  // Tuning Parameters (Initial Conservative Guesses)
  double _kp = 2.0, _ki = 0.5, _kd = 1.0;

  void applyTimeProportional(int pin, double output);
};

#endif
