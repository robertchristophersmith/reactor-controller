#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Communications ---
#define SERIAL_BAUD 115200

// --- SPI Bus (MAX31855 Thermocouples) ---
// Hardware SPI: SCK=52, MISO=50
#define PIN_SPI_CS_TC_GAS_INTERNAL 22   // Gas Preheat
#define PIN_SPI_CS_TC_VAPORIZER_WALL 23 // Vaporizer Wall
#define PIN_SPI_CS_TC_REACTOR_EXT_1 24  // Reactor Zone 1 External
#define PIN_SPI_CS_TC_REACTOR_EXT_2 25  // Reactor Zone 2 External
#define PIN_SPI_CS_TC_REACTOR_INT_1 26  // Reactor Zone 1 Internal
#define PIN_SPI_CS_TC_REACTOR_INT_2 27  // Reactor Zone 2 Internal
#define PIN_SPI_CS_TC_FEEDSTOCK                                                \
  28 // Feedstock (Moved to 28, previously used by Ext2)

// --- I2C Bus (ADS1115, MCP4725) ---
// Hardware I2C: SDA=20, SCL=21
#define I2C_ADDR_ADS1115_MFC 0x48      // GND - MFC Feedback
#define I2C_ADDR_ADS1115_PRESSURE 0x49 // VDD - Pressure Transducer
#define I2C_ADDR_ADS1115_H2 0x4A       // SDA - Hydrogen Sensor
#define I2C_ADDR_MFC_DAC 0x62          // Default Adafruit Address

// --- ADS1115 Channel Map ---
#define ADC_CH_MFC_FLOW_READ 0 // On ADS_MFC
#define ADC_CH_PRESSURE 0      // On ADS_PRESSURE (0-30psig)
#define ADC_CH_H2_SENSOR 2     // On ADS_H2 (MQ-8)

// --- Flow Constants ---
#define MFC_FLOW_MAX_SCCM 3000.0 // Full scale flow
#define MFC_VOLTAGE_MIN 0.5
#define MFC_VOLTAGE_MAX 4.5
#define PRESSURE_MAX_PSIG 30.0
#define H2_MAX_PERCENT 100.0

// --- Actuators (Heaters - SSRs) ---
#define PIN_HEATER_GAS 6       // PWM capable
#define PIN_HEATER_VAPORIZER 7 // PWM capable
#define PIN_HEATER_REACTOR_1 8 // PWM capable
#define PIN_HEATER_REACTOR_2 9 // PWM capable

// --- Safety Limits ---
#define MAX_TEMP_C_GAS 500.0
#define MAX_TEMP_C_REACTOR 800.0
#define MAX_PRESSURE_BAR 10.0

// --- Control Loop ---
#define LOOP_INTERVAL_MS 100 // 10Hz Control Loop

#endif
