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
        if (!typeStr) {
          sendError("Missing 'cmd' field");
          return cmd;
        }
        if (strcmp(typeStr, "SET_TEMP") == 0) {
          cmd.type = CMD_SET_TEMP;
          cmd.zone = doc["zone"];
          cmd.value = doc["val"];
        } else if (strcmp(typeStr, "SET_STATE") == 0) {
          cmd.type = CMD_SET_STATE;
          cmd.state = doc["state"];

        } else if (strcmp(typeStr, "HEARTBEAT") == 0) {
          cmd.type = CMD_HEARTBEAT;
        } else if (strcmp(typeStr, "TARE_LOADCELL") == 0) {
          cmd.type = CMD_TARE_LOADCELL;
        } else if (strcmp(typeStr, "CALIBRATE_LOADCELL") == 0) {
          cmd.type = CMD_CALIBRATE_LOADCELL;
          cmd.value = doc["val"];
        } else if (strcmp(typeStr, "PUMP_CONTROL") == 0) {
          cmd.type = CMD_PUMP_CONTROL;
          cmd.zone = doc["id"];
          cmd.state = doc["state"];
          cmd.dir = doc["dir"];
          cmd.value = doc["speed"];
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
                                HeaterController &heaters,
                                ControlState state, unsigned long uptime) {
  StaticJsonDocument<512> doc;

  doc["uptime"] = uptime;
  doc["state"] = state;

  // Sensors (Only sending HX711 and Status)
  JsonObject s = doc.createNestedObject("sensors");
  s["status"] = sensors.sensorStatus;
  s["weight"] = sensors.weightKg;

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

  serializeJson(doc, Serial);
  Serial.println();
}

void SerialComms::sendError(const String &msg) {
  StaticJsonDocument<128> doc;
  doc["error"] = msg;
  serializeJson(doc, Serial);
  Serial.println();
}
