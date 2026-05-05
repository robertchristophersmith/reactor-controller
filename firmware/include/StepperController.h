#ifndef STEPPER_CONTROLLER_H
#define STEPPER_CONTROLLER_H

#include <Arduino.h>
#include <ModbusMaster.h>

class StepperController {
public:
  StepperController(uint8_t address, float mlPerRev = 1.0);
  void begin(Stream &serial);

  // Core Motion
  void setFlowRate(float mlPerMin); // Calculates RPM based on mlPerRev
  void setSpeed(float rpm);         // Writes to register 0x0105
  void start(bool forward = true);  // Starts motor in continuous mode
  void stop();

  // Safety
  void emergencyStop();

  // Helper
  uint8_t getAddress() const { return _address; }

private:
  uint8_t _address;
  float _mlPerRev;
  ModbusMaster _node;
};

#endif
