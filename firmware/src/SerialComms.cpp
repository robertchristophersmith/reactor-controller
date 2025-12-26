#include "SerialComms.h"

SerialComms::SerialComms() { _bufIndex = 0; }

void SerialComms::begin() {
  // Serial begin handled in main/setup usually, but we can ensure it here if
  // needed relying on global Serial
}

Command SerialComms::checkCommand() {
  Command cmd;
  cmd.type = CMD_NONE;

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      _readBuffer[_bufIndex] = '\0';
      _bufIndex = 0;

      // Parse JSON
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, _readBuffer);

      if (!error) {
        const char *typeStr = doc["cmd"];
        if (strcmp(typeStr, "SET_TEMP") == 0) {
          cmd.type = CMD_SET_TEMP;
          cmd.zone = doc["zone"];
          cmd.value = doc["val"];
        } else if (strcmp(typeStr, "SET_STATE") == 0) {
          cmd.type = CMD_SET_STATE;
          cmd.state = doc["state"];
        } else if (strcmp(typeStr, "SET_FLOW") == 0) {
          cmd.type = CMD_SET_FLOW;
          cmd.value = doc["val"];
        } else if (strcmp(typeStr, "HEARTBEAT") == 0) {
          cmd.type = CMD_HEARTBEAT;
        }
        return cmd; // Return immediately on full command
      } else {
        sendError("JSON Parse Error");
      }
    } else {
      if (_bufIndex < 255) {
        _readBuffer[_bufIndex++] = c;
      }
    }
  }
  return cmd;
}

void SerialComms::sendTelemetry(const SensorData &sensors,
                                HeaterController &heaters, FlowController &flow,
                                ControlState state, unsigned long uptime) {
  StaticJsonDocument<512> doc;

  doc["uptime"] = uptime;
  doc["state"] = state;

  // Sensors
  JsonObject s = doc.createNestedObject("sensors");
  s["t_gas"] = sensors.tempGasInternal;
  s["t_feed"] = sensors.tempFeedstock;
  s["t_vap"] = sensors.tempVaporizerWall;
  s["t_r_i1"] = sensors.tempReactorInt1;
  s["t_r_i2"] = sensors.tempReactorInt2;
  s["t_r_e1"] = sensors.tempReactorExt1;
  s["t_r_e2"] = sensors.tempReactorExt2;
  s["p_feed"] = sensors.pressureFeedBar;
  s["p_reac"] = sensors.pressureReactorBar;
  s["flow"] = sensors.flowRateSccm;
  s["h2"] = sensors.h2ConcentrationPpm;

  // Heaters
  JsonObject h = doc.createNestedObject("heaters");
  h["gas"] = heaters.getOutputGas();
  h["vap"] = heaters.getOutputVaporizer();
  h["reac1"] = heaters.getOutputReactor1();
  h["reac2"] = heaters.getOutputReactor2();

  // Setpoints
  JsonObject sp = doc.createNestedObject("sp");
  sp["gas"] = heaters.getSetpointGas();
  sp["vap"] = heaters.getSetpointVaporizer();
  sp["reac1"] = heaters.getSetpointReactor1();
  sp["reac2"] = heaters.getSetpointReactor2();
  sp["flow"] = flow.getSetpoint();

  serializeJson(doc, Serial);
  Serial.println();
}

void SerialComms::sendError(const String &msg) {
  StaticJsonDocument<128> doc;
  doc["error"] = msg;
  serializeJson(doc, Serial);
  Serial.println();
}
