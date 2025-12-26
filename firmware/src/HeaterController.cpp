#include "HeaterController.h"

HeaterController::HeaterController() {
  // Initialize Variables
  _spGas = 0;
  _inGas = 0;
  _outGas = 0;
  _spVaporizer = 0;
  _inVaporizer = 0;
  _outVaporizer = 0;
  _spReactor1 = 0;
  _inReactor1 = 0;
  _outReactor1 = 0;
  _spReactor2 = 0;
  _inReactor2 = 0;
  _outReactor2 = 0;

  _enabled = false;
  _windowStartTime = millis();

  // P_ON_M specifies Proportional on Measurement (reduces overshoot) if
  // supported, but standard constructor is (&Input, &Output, &Setpoint, Kp, Ki,
  // Kd, Direction)
  _pidGas = new PID(&_inGas, &_outGas, &_spGas, _kp, _ki, _kd, DIRECT);
  _pidVaporizer = new PID(&_inVaporizer, &_outVaporizer, &_spVaporizer, _kp,
                          _ki, _kd, DIRECT);
  _pidReactor1 =
      new PID(&_inReactor1, &_outReactor1, &_spReactor1, _kp, _ki, _kd, DIRECT);
  _pidReactor2 =
      new PID(&_inReactor2, &_outReactor2, &_spReactor2, _kp, _ki, _kd, DIRECT);
}

void HeaterController::begin() {
  pinMode(PIN_HEATER_GAS, OUTPUT);
  pinMode(PIN_HEATER_VAPORIZER, OUTPUT);
  pinMode(PIN_HEATER_REACTOR_1, OUTPUT);
  pinMode(PIN_HEATER_REACTOR_2, OUTPUT);

  digitalWrite(PIN_HEATER_GAS, LOW);
  digitalWrite(PIN_HEATER_VAPORIZER, LOW);
  digitalWrite(PIN_HEATER_REACTOR_1, LOW);
  digitalWrite(PIN_HEATER_REACTOR_2, LOW);

  // Limit output to 0-WINDOW_SIZE (time proportional)
  _pidGas->SetOutputLimits(0, WINDOW_SIZE);
  _pidVaporizer->SetOutputLimits(0, WINDOW_SIZE);
  _pidReactor1->SetOutputLimits(0, WINDOW_SIZE);
  _pidReactor2->SetOutputLimits(0, WINDOW_SIZE);

  _pidGas->SetMode(AUTOMATIC);
  _pidVaporizer->SetMode(AUTOMATIC);
  _pidReactor1->SetMode(AUTOMATIC);
  _pidReactor2->SetMode(AUTOMATIC);
}

void HeaterController::setSetpoints(float spGas, float spVaporizer,
                                    float spReactor1, float spReactor2) {
  _spGas = spGas;
  _spVaporizer = spVaporizer;
  _spReactor1 = spReactor1;
  _spReactor2 = spReactor2;
}

void HeaterController::setEnabled(bool enabled) {
  _enabled = enabled;
  if (!enabled) {
    // Force outputs off immediately
    digitalWrite(PIN_HEATER_GAS, LOW);
    digitalWrite(PIN_HEATER_VAPORIZER, LOW);
    digitalWrite(PIN_HEATER_REACTOR_1, LOW);
    digitalWrite(PIN_HEATER_REACTOR_2, LOW);

    // Reset PID integral terms? Usually good practice, or set mode to MANUAL.
    // For simplicity:
    _pidGas->SetMode(MANUAL);
    _pidVaporizer->SetMode(MANUAL);
    _pidReactor1->SetMode(MANUAL);
    _pidReactor2->SetMode(MANUAL);
    _outGas = 0;
    _outVaporizer = 0;
    _outReactor1 = 0;
    _outReactor2 = 0;
  } else {
    _pidGas->SetMode(AUTOMATIC);
    _pidVaporizer->SetMode(AUTOMATIC);
    _pidReactor1->SetMode(AUTOMATIC);
    _pidReactor2->SetMode(AUTOMATIC);
  }
}

void HeaterController::update(float tempGas, float tempVaporizer,
                              float tempReactor1, float tempReactor2) {
  if (!_enabled) {
    digitalWrite(PIN_HEATER_GAS, LOW);
    digitalWrite(PIN_HEATER_VAPORIZER, LOW);
    digitalWrite(PIN_HEATER_REACTOR_1, LOW);
    digitalWrite(PIN_HEATER_REACTOR_2, LOW);
    return;
  }

  _inGas = tempGas;
  _inVaporizer = tempVaporizer;
  _inReactor1 = tempReactor1;
  _inReactor2 = tempReactor2;

  _pidGas->Compute();
  _pidVaporizer->Compute();
  _pidReactor1->Compute();
  _pidReactor2->Compute();

  // Time Proportional Logic
  unsigned long now = millis();
  if (now - _windowStartTime > WINDOW_SIZE) {
    _windowStartTime += WINDOW_SIZE;
  }

  applyTimeProportional(PIN_HEATER_GAS, _outGas);
  applyTimeProportional(PIN_HEATER_VAPORIZER, _outVaporizer);
  applyTimeProportional(PIN_HEATER_REACTOR_1, _outReactor1);
  applyTimeProportional(PIN_HEATER_REACTOR_2, _outReactor2);
}

void HeaterController::applyTimeProportional(int pin, double output) {
  unsigned long now = millis();
  if (output > (now - _windowStartTime)) {
    digitalWrite(pin, HIGH);
  } else {
    digitalWrite(pin, LOW);
  }
}
