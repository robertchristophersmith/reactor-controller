#ifndef SERIAL_COMMS_H
#define SERIAL_COMMS_H

#include "FlowController.h"
#include "SensorManager.h"
#include <Arduino.h>
#include <ArduinoJson.h>


enum CommandType {
  CMD_NONE,
  CMD_SET_TEMP,
  CMD_SET_STATE,
  CMD_HEARTBEAT,
  CMD_SET_FLOW
};
enum ControlState {
  STATE_STANDBY,
  STATE_WARMUP,
  STATE_WORKING,
  STATE_ALARM,
  STATE_FAULT
};

struct Command {
  CommandType type;
  int zone; // 0=Gas, 1=Vap, 2=Reactor
  float value;
  int state;
};

class SerialComms {
public:
  SerialComms();
  void begin();
  Command checkCommand();
  void sendTelemetry(const SensorData &sensors, HeaterController &heaters,
                     FlowController &flow, ControlState state,
                     unsigned long uptime);
  void sendError(const String &msg);

private:
  char _readBuffer[256];
  int _bufIndex;
};

#endif
