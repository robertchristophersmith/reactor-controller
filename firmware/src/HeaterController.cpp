#include "HeaterController.h"

HeaterController::HeaterController() {
  // Initialize Variables
  _spPreheater = 0;
  _inPreheater = 0;
  _outPreheater = 0;
  _spLiquid = 0;
  _inLiquid = 0;
  _outLiquid = 0;
  _spGas = 0;
  _inGas = 0;
  _outGas = 0;

  _enabled = false;
  _windowStartTime = millis();

  // Initialize PIDs
  _pidPreheater = new PID(&_inPreheater, &_outPreheater, &_spPreheater, _kp, _ki, _kd, DIRECT);
  _pidLiquid = new PID(&_inLiquid, &_outLiquid, &_spLiquid, _kp, _ki, _kd, DIRECT);
  _pidGas = new PID(&_inGas, &_outGas, &_spGas, _kp, _ki, _kd, DIRECT);
}

void HeaterController::begin() {
  pinMode(PIN_HEATER_FEEDSTOCK_PREHEATER, OUTPUT);
  pinMode(PIN_HEATER_LIQUID_REACTOR, OUTPUT);
  pinMode(PIN_HEATER_GAS_REACTOR, OUTPUT);

  digitalWrite(PIN_HEATER_FEEDSTOCK_PREHEATER, LOW);
  digitalWrite(PIN_HEATER_LIQUID_REACTOR, LOW);
  digitalWrite(PIN_HEATER_GAS_REACTOR, LOW);

  // Limit output to 0-WINDOW_SIZE (time proportional)
  _pidPreheater->SetOutputLimits(0, WINDOW_SIZE);
  _pidLiquid->SetOutputLimits(0, WINDOW_SIZE);
  _pidGas->SetOutputLimits(0, WINDOW_SIZE);

  _pidPreheater->SetMode(AUTOMATIC);
  _pidLiquid->SetMode(AUTOMATIC);
  _pidGas->SetMode(AUTOMATIC);
}

void HeaterController::setSetpoints(float spPreheater, float spLiquid, float spGas) {
  _spPreheater = spPreheater;
  _spLiquid = spLiquid;
  _spGas = spGas;
}

void HeaterController::setEnabled(bool enabled) {
  _enabled = enabled;
  if (!enabled) {
    // Force outputs off immediately
    digitalWrite(PIN_HEATER_FEEDSTOCK_PREHEATER, LOW);
    digitalWrite(PIN_HEATER_LIQUID_REACTOR, LOW);
    digitalWrite(PIN_HEATER_GAS_REACTOR, LOW);

    _pidPreheater->SetMode(MANUAL);
    _pidLiquid->SetMode(MANUAL);
    _pidGas->SetMode(MANUAL);
    _outPreheater = 0;
    _outLiquid = 0;
    _outGas = 0;
  } else {
    _pidPreheater->SetMode(AUTOMATIC);
    _pidLiquid->SetMode(AUTOMATIC);
    _pidGas->SetMode(AUTOMATIC);
  }
}

void HeaterController::update(float tempPreheater, float tempLiquid, float tempGas) {
  if (!_enabled) {
    digitalWrite(PIN_HEATER_FEEDSTOCK_PREHEATER, LOW);
    digitalWrite(PIN_HEATER_LIQUID_REACTOR, LOW);
    digitalWrite(PIN_HEATER_GAS_REACTOR, LOW);
    return;
  }

  _inPreheater = tempPreheater;
  _inLiquid = tempLiquid;
  _inGas = tempGas;

  _pidPreheater->Compute();
  _pidLiquid->Compute();
  _pidGas->Compute();

  // Time Proportional Logic
  unsigned long now = millis();
  if (now - _windowStartTime > WINDOW_SIZE) {
    _windowStartTime += WINDOW_SIZE;
  }

  applyTimeProportional(PIN_HEATER_FEEDSTOCK_PREHEATER, _outPreheater);
  applyTimeProportional(PIN_HEATER_LIQUID_REACTOR, _outLiquid);
  applyTimeProportional(PIN_HEATER_GAS_REACTOR, _outGas);
}

void HeaterController::applyTimeProportional(int pin, double output) {
  unsigned long now = millis();
  if (output > (now - _windowStartTime)) {
    digitalWrite(pin, HIGH);
  } else {
    digitalWrite(pin, LOW);
  }
}
