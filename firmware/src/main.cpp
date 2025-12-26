#include "HeaterController.h"
#include "SensorManager.h"
#include "SerialComms.h"
#include "WeightedAverage.h"
#include "config.h"
#include <Arduino.h>

// --- Global Objects ---
SensorManager sensors;
HeaterController heaters;
FlowController flow;
SerialComms comms;
WeightedAverage wAvg1; // Zone 1
WeightedAverage wAvg2; // Zone 2

// --- State Management ---
ControlState currentState = STATE_STANDBY;
unsigned long lastLoopTime = 0;
unsigned long lastTelemetryTime = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long startTime = 0;

const unsigned long HEARTBEAT_TIMEOUT = 5000; // 5 Seconds safety timeout

// --- Forward Declarations ---
void updateFSM(SensorData &data);
void checkSafety(SensorData &data);

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial)
    delay(10); // Wait for USB

  sensors.begin();
  heaters.begin();
  flow.begin();
  comms.begin();

  startTime = millis();
  lastHeartbeatTime = millis();

  // Initial state
  currentState = STATE_STANDBY;
}

void loop() {
  unsigned long now = millis();

  // 1. Process Serial Commands (As fast as possible)
  Command cmd = comms.checkCommand();
  if (cmd.type != CMD_NONE) {
    lastHeartbeatTime = now; // Reset watchdog

    switch (cmd.type) {
    case CMD_SET_TEMP:
      if (cmd.zone == 0)
        heaters.setSetpoints(cmd.value, heaters.getSetpointVaporizer(),
                             heaters.getSetpointReactor());
      if (cmd.zone == 1)
        heaters.setSetpoints(heaters.getSetpointGas(), cmd.value,
                             heaters.getSetpointReactor());
      if (cmd.zone == 2)
        heaters.setSetpoints(heaters.getSetpointGas(),
                             heaters.getSetpointVaporizer(), cmd.value,
                             heaters.getSetpointReactor2());
      if (cmd.zone == 3)
        heaters.setSetpoints(heaters.getSetpointGas(),
                             heaters.getSetpointVaporizer(),
                             heaters.getSetpointReactor1(), cmd.value);
      break;
    case CMD_SET_STATE:
      currentState = (ControlState)cmd.state;
      break;
    case CMD_SET_FLOW:
      flow.setFlow(cmd.value);
      break;
    case CMD_HEARTBEAT:
      break;
    }
  }

  // 2. Fixed Interval Control Loop (10Hz)
  if (now - lastLoopTime >= LOOP_INTERVAL_MS) {
    lastLoopTime = now;

    // A. Read Sensors
    sensors.update();
    SensorData data = sensors.getLastReadings();

    // B. Check Safety (Hard Limits)
    checkSafety(data);

    // C. Update FSM (Logic for each state)
    updateFSM(data);

    // Calc Weighted PVs (Simplest: 50/50 split of Int/Ext)
    float instant1 = (data.tempReactorInt1 + data.tempReactorExt1) / 2.0;
    float instant2 = (data.tempReactorInt2 + data.tempReactorExt2) / 2.0;

    wAvg1.add(instant1);
    wAvg2.add(instant2);

    float pv1 = wAvg1.getAverage();
    float pv2 = wAvg2.getAverage();

    // D. Update Heaters (PID calculation)
    heaters.update(data.tempGasInternal, data.tempVaporizerWall, pv1, pv2);

    // E. Telemetry (1Hz)
    if (now - lastTelemetryTime >= 1000) {
      lastTelemetryTime = now;
      comms.sendTelemetry(data, heaters, flow, currentState,
                          (now - startTime) / 1000);
    }
  }

  // 3. Watchdog Check
  if (now - lastHeartbeatTime > HEARTBEAT_TIMEOUT) {
    if (currentState != STATE_FAULT && currentState != STATE_ALARM &&
        currentState != STATE_STANDBY) {
      currentState = STATE_ALARM;
      comms.sendError("HEARTBEAT_TIMEOUT");
    }
  }
}

void checkSafety(SensorData &data) {
  // Immediate overrides regardless of state
  if (data.tempGasInternal > MAX_TEMP_C_GAS ||
      data.tempReactorInt1 > MAX_TEMP_C_REACTOR ||
      data.pressureReactorBar > MAX_PRESSURE_BAR) {

    if (currentState != STATE_FAULT) {
      currentState = STATE_FAULT;
      comms.sendError("SAFETY_LIMIT_EXCEEDED");
    }
  }

  if (!data.sensorsHealthy) {
    if (currentState != STATE_FAULT) {
      currentState = STATE_FAULT;
      comms.sendError("SENSOR_FAILURE");
    }
  }
}

void updateFSM(SensorData &data) {
  switch (currentState) {
  case STATE_STANDBY:
    heaters.setEnabled(false);
    flow.setEnabled(false);
    break;

  case STATE_WARMUP:
    heaters.setEnabled(true);
    flow.setEnabled(true);
    break;

  case STATE_WORKING:
    heaters.setEnabled(true);
    flow.setEnabled(true);
    break;

  case STATE_ALARM:
    heaters.setEnabled(false);
    flow.setEnabled(false);
    break;

  case STATE_FAULT:
    heaters.setEnabled(false);
    flow.setEnabled(false);
    break;
  }
}
