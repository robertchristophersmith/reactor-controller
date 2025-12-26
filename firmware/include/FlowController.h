#ifndef FLOWCONTROLLER_H
#define FLOWCONTROLLER_H

#include "config.h"
#include <Adafruit_MCP4725.h>
#include <Arduino.h>

class FlowController {
public:
  FlowController();
  bool begin();
  void setFlow(float sccm);
  void setEnabled(bool enabled);
  float getSetpoint();

private:
  Adafruit_MCP4725 dac;
  float currentSetpointSCCM;
  bool _enabled;
};

#endif
