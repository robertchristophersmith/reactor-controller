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
  void update(float tempGas, float tempVaporizer, float tempReactor);
  void setSetpoints(float spGas, float spVaporizer, float spReactor1,
                    float spReactor2);
  void update(float tempGas, float tempVaporizer, float tempReactor1,
              float tempReactor2);
  void setEnabled(bool enabled);

  // Telemetry getters
  float getOutputGas() { return _outGas; }
  float getOutputVaporizer() { return _outVaporizer; }
  float getOutputReactor1() { return _outReactor1; }
  float getOutputReactor2() { return _outReactor2; }

  float getSetpointGas() { return _spGas; }
  float getSetpointVaporizer() { return _spVaporizer; }
  float getSetpointReactor1() { return _spReactor1; }
  float getSetpointReactor2() { return _spReactor2; }

private:
  bool _enabled;
  unsigned long _windowStartTime;

  // PID Variables (Double required by Library)
  double _spGas, _inGas, _outGas;
  double _spVaporizer, _inVaporizer, _outVaporizer;
  double _spReactor1, _inReactor1, _outReactor1;
  double _spReactor2, _inReactor2, _outReactor2;

  // PID Objects
  PID *_pidGas;
  PID *_pidVaporizer;
  PID *_pidReactor1;
  PID *_pidReactor2;

  // Tuning Parameters (Initial Conservative Guesses)
  double _kp = 2.0, _ki = 0.5, _kd = 1.0;

  void applyTimeProportional(int pin, double output);
};

#endif
