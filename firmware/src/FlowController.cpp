#include "FlowController.h"

FlowController::FlowController() {
  currentSetpointSCCM = 0.0;
  _enabled = false;
}

bool FlowController::begin() {
  // Initialize DAC with default address
  bool res = dac.begin(I2C_ADDR_MFC_DAC);
  setEnabled(false); // Default to off
  return res;
}

void FlowController::setFlow(float sccm) {
  // Constrain flow to 0 - MAX
  float constrained = constrain(sccm, 0.0, MFC_FLOW_MAX_SCCM);
  currentSetpointSCCM = constrained;

  // If disabled, don't write to DAC (or write 0)
  if (_enabled) {
    // Convert SCCM to Voltage (0.5V - 4.5V)
    // 0 SCCM = 0.5V
    // MAX SCCM = 4.5V
    float targetVoltage =
        MFC_VOLTAGE_MIN +
        (constrained / MFC_FLOW_MAX_SCCM) * (MFC_VOLTAGE_MAX - MFC_VOLTAGE_MIN);

    // Clamp
    if (targetVoltage < MFC_VOLTAGE_MIN)
      targetVoltage = MFC_VOLTAGE_MIN;
    if (targetVoltage > MFC_VOLTAGE_MAX)
      targetVoltage = MFC_VOLTAGE_MAX;

    // Convert Voltage to DAC counts (12-bit, 0-5V ref)
    // 4095 = 5.0V
    uint16_t dacValue = (uint16_t)((targetVoltage / 5.0) * 4095.0);

    // Apply
    // Write to DAC (false = don't write to EEPROM, faster)
    dac.setVoltage(dacValue, false);
  } else {
    dac.setVoltage(0, false);
  }
}

void FlowController::setEnabled(bool enabled) {
  _enabled = enabled;
  // Re-apply current setpoint (which will output 0 if disabled)
  setFlow(currentSetpointSCCM);
}

float FlowController::getSetpoint() { return currentSetpointSCCM; }
