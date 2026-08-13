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

  // Sensors
  JsonObject s = doc.createNestedObject("sensors");
  s["status"] = sensors.sensorStatus;
  s["weight"] = sensors.weightKg;

  auto setTc = [&](const char *key, float val) {
    if (isnan(val)) {
      s[key] = nullptr;
    } else {
      s[key] = val;
    }
  };

  setTc("t_feed_res", sensors.tempFeedstockReservoir);
  setTc("t_feed_pre", sensors.tempFeedstockPreheater);
  setTc("t_liq_reac", sensors.tempLiquidReactor);
  setTc("t_gas_reac_int", sensors.tempGasReactorInt);
  setTc("t_gas_reac_ext", sensors.tempGasReactorExt);
  setTc("t_elec_housing", sensors.tempElectronicsHousing);
  s["h2"] = sensors.h2ConcentrationPpm;

  JsonObject errs = s.createNestedObject("tc_errors");
  errs["t_feed_res"] = sensors.errFeedstockReservoir;
  errs["t_feed_pre"] = sensors.errFeedstockPreheater;
  errs["t_liq_reac"] = sensors.errLiquidReactor;
  errs["t_gas_reac_int"] = sensors.errGasReactorInt;
  errs["t_gas_reac_ext"] = sensors.errGasReactorExt;
  errs["t_elec_housing"] = sensors.errElectronicsHousing;

  // Heaters
  JsonObject h = doc.createNestedObject("heaters");
  h["feed_pre"] = heaters.getOutputPreheater();
  h["liq_reac"] = heaters.getOutputLiquid();
  h["gas_reac"] = heaters.getOutputGas();

  // Setpoints
  JsonObject sp = doc.createNestedObject("sp");
  sp["feed_pre"] = heaters.getSetpointPreheater();
  sp["liq_reac"] = heaters.getSetpointLiquid();
  sp["gas_reac"] = heaters.getSetpointGas();

  serializeJson(doc, Serial);
  Serial.println();
}

void SerialComms::sendError(const String &msg) {
  StaticJsonDocument<128> doc;
  doc["error"] = msg;
  serializeJson(doc, Serial);
  Serial.println();
}
