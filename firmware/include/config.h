#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Communications ---
#define SERIAL_BAUD 115200

// --- SPI Bus (Temperature Sensors) ---
// Software SPI on Pins 7 (CLK) and 5 (MISO) for remaining MAX31855 Thermocouples (Mega Pin 50/52 bypass)
#ifdef PIN_SPI_SCK
#undef PIN_SPI_SCK
#endif
#define PIN_SPI_SCK 7

#ifdef PIN_SPI_MISO
#undef PIN_SPI_MISO
#endif
#define PIN_SPI_MISO 5

// Thermocouple Channels (MAX31855)
#define PIN_SPI_CS_TC_FEEDSTOCK_RESERVOIR 38 // Feedstock reservoir
#define PIN_SPI_CS_TC_ELECTRONICS_HOUSING 36 // Electronics housing temperature monitor

// RTD Channels (PWFusion SEN-30203 Quad MAX31865 PT100)
#define PIN_SPI_CS_RTD_FEEDSTOCK_PREHEATER 40  // External feedstock preheater (RTD0)
#define PIN_SPI_CS_RTD_LIQUID_REACTOR      42  // Internal liquid phase reactor (RTD1)
#define PIN_SPI_CS_RTD_GAS_REACTOR_INT     44  // Internal gas phase reactor (RTD2)
#define PIN_SPI_CS_RTD_GAS_REACTOR_EXT     46  // External gas phase reactor (RTD3)

// Backward-compatible alias definitions
#define PIN_SPI_CS_RTD_PREHEATER     PIN_SPI_CS_RTD_FEEDSTOCK_PREHEATER
#define PIN_SPI_CS_RTD_LIQUID_REAC   PIN_SPI_CS_RTD_LIQUID_REACTOR
#define PIN_SPI_CS_RTD_GAS_REAC_INT  PIN_SPI_CS_RTD_GAS_REACTOR_INT
#define PIN_SPI_CS_RTD_GAS_REAC_EXT  PIN_SPI_CS_RTD_GAS_REACTOR_EXT
#define PIN_SPI_CS_TC_FEEDSTOCK_PREHEATER PIN_SPI_CS_RTD_FEEDSTOCK_PREHEATER
#define PIN_SPI_CS_TC_LIQUID_REACTOR PIN_SPI_CS_RTD_LIQUID_REACTOR
#define PIN_SPI_CS_TC_GAS_REACTOR_INT PIN_SPI_CS_RTD_GAS_REACTOR_INT
#define PIN_SPI_CS_TC_GAS_REACTOR_EXT PIN_SPI_CS_RTD_GAS_REACTOR_EXT
// --- Analog Inputs ---
#define PIN_H2_SENSOR A0 // MQ-8 Hydrogen Sensor (Analog Out)

// --- Load Cell (HX711) ---
#define PIN_HX711_SCK 2
#define PIN_HX711_DT 3

// --- Stepper Motors (Modbus TTL) ---
#define STEPPER_ADDR_FEEDSTOCK 1
#define STEPPER_ADDR_AUX 2
#define STEPPER_MODBUS_BAUD 9600
// Registers
#define REG_RUN_STOP 0x0100
#define REG_CONTINUOUS_MODE 0x0101
#define REG_ACCEL_DECEL 0x0104
#define REG_SPEED 0x0105

// --- Constants ---
#define H2_MAX_PERCENT 100.0

// --- Actuators (Heaters - SSRs) ---
#define PIN_HEATER_FEEDSTOCK_PREHEATER 6 // PWM capable
#define PIN_HEATER_LIQUID_REACTOR 8      // PWM capable
#define PIN_HEATER_GAS_REACTOR 9         // PWM capable

// --- Safety Limits ---
#define MAX_TEMP_C_PREHEATER 300.0
#define MAX_TEMP_C_LIQUID_REACTOR 500.0
#define MAX_TEMP_C_GAS_REACTOR 800.0
#define MAX_TEMP_C_HOUSING 90.0

// --- Control Loop ---
#define LOOP_INTERVAL_MS 100 // 10Hz Control Loop

#endif
