#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Communications ---
#define SERIAL_BAUD 115200

// --- SPI Bus (MAX31855 Thermocouples) ---
// Software SPI on Pins 7 (CLK) and 5 (MISO) to bypass damaged Mega Pin 50/52
#ifdef PIN_SPI_SCK
#undef PIN_SPI_SCK
#endif
#define PIN_SPI_SCK 7

#ifdef PIN_SPI_MISO
#undef PIN_SPI_MISO
#endif
#define PIN_SPI_MISO 5
#define PIN_SPI_CS_TC_FEEDSTOCK_RESERVOIR 28 // Feedstock reservoir
#define PIN_SPI_CS_TC_FEEDSTOCK_PREHEATER 22  // External feedstock preheater
#define PIN_SPI_CS_TC_LIQUID_REACTOR 25       // Internal liquid phase reactor
#define PIN_SPI_CS_TC_GAS_REACTOR_INT 27      // Internal gas phase reactor
#define PIN_SPI_CS_TC_GAS_REACTOR_EXT 26      // External gas phase reactor
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

// --- Control Loop ---
#define LOOP_INTERVAL_MS 100 // 10Hz Control Loop

#endif
