#include "StepperController.h"
#include "config.h"

StepperController::StepperController(uint8_t address, float mlPerRev) {
  _address = address;
  _mlPerRev = mlPerRev;
  if (_mlPerRev <= 0)
    _mlPerRev = 1.0; // Safety
}

void StepperController::begin(Stream &serial) { _node.begin(_address, serial); }

void StepperController::setFlowRate(float mlPerMin) {
  float rpm = mlPerMin / _mlPerRev;
  setSpeed(rpm);
}

void StepperController::setSpeed(float rpm) {
  // Register 0x0105: Speed (RPM * 10)
  // Ensure RPM is positive
  if (rpm < 0)
    rpm = 0;
  uint16_t speedVal = (uint16_t)(rpm * 10.0f);
  _node.writeSingleRegister(REG_SPEED, speedVal);
}

void StepperController::start() {
  // 1. Enable Continuous Mode (0x0101 = 1)
  _node.writeSingleRegister(REG_CONTINUOUS_MODE, 1);
  // 2. Run Forward (0x0100 = 1)
  // Assuming 1 is Run Forward.
  _node.writeSingleRegister(REG_RUN_STOP, 1);
}

void StepperController::stop() {
  // Stop (0x0100 = 0)
  _node.writeSingleRegister(REG_RUN_STOP, 0);
}

void StepperController::emergencyStop() {
  // Force stop
  stop();
}
