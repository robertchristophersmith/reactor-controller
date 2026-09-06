#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include <PwFusion_MAX31865.h>

struct RtdChannel {
  const char *name;
  uint8_t csPin;
  MAX31865 sensor;
};

RtdChannel channels[] = {
    {"Feedstock Preheater (RTD0)", 40, MAX31865()},
    {"Liquid Reactor     (RTD1)", 42, MAX31865()},
    {"Gas Reactor Int    (RTD2)", 44, MAX31865()},
    {"Gas Reactor Ext    (RTD3)", 46, MAX31865()}
};

const int NUM_CHANNELS = sizeof(channels) / sizeof(channels[0]);

float calculateCvdTemperature(float rOhms) {
  if (isnan(rOhms) || rOhms <= 0.0f) {
    return NAN;
  }
  const float R0 = 100.0f;
  const float A = 3.9083e-3f;
  const float B = -5.775e-7f;

  float discriminant = (A * A) - (4.0f * B * (1.0f - (rOhms / R0)));
  if (discriminant < 0.0f) {
    return NAN;
  }
  return (-A + sqrt(discriminant)) / (2.0f * B);
}

void printFaultDetails(uint8_t status) {
  if (status == 0) {
    Serial.print("[OK]");
    return;
  }
  Serial.print("[FAULT: 0x");
  if (status < 0x10) Serial.print("0");
  Serial.print(status, HEX);
  Serial.print(" -> ");

  bool first = true;
  auto printBit = [&](uint8_t mask, const char *msg) {
    if (status & mask) {
      if (!first) Serial.print(", ");
      Serial.print(msg);
      first = false;
    }
  };

  printBit(RTD_FAULT_VOLTAGE_OOR, "Voltage OOR / Overvoltage (0x04)");
  printBit(RTD_FAULT_RTDIN_LOW_OPEN, "RTDIn- / Force- Open (0x08)");
  printBit(RTD_FAULT_REFIN_LOW_OPEN, "RefIn- Low / Open (0x10)");
  printBit(RTD_FAULT_REFIN_HIGH, "RefIn- High (0x20)");
  printBit(RTD_FAULT_TEMP_LOW, "Low Temp Threshold (0x40)");
  printBit(RTD_FAULT_TEMP_HIGH, "High Temp Threshold (0x80)");
  Serial.print("]");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  Serial.println("\n===========================================================");
  Serial.println("   SEN-30203 Quad PT100 RTD MAX31865 Diagnostic Scanner   ");
  Serial.println("   Hardware SPI: SCK=52, MISO=50, MOSI=51                  ");
  Serial.println("===========================================================");

  // ATmega2560 SS Pin 53 must be OUTPUT HIGH
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);

  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV16);

  for (int i = 0; i < NUM_CHANNELS; i++) {
    pinMode(channels[i].csPin, OUTPUT);
    digitalWrite(channels[i].csPin, HIGH);
    channels[i].sensor.begin(channels[i].csPin, RTD_3_WIRE, RTD_TYPE_PT100, SPI);
  }

  Serial.println("Initialized 4 RTD Channels (3-Wire PT100, Rref=400 Ohm).");
  delay(500);
}

void loop() {
  Serial.println("\n-----------------------------------------------------------");
  Serial.println("  Sampling All 4 MAX31865 RTD Channels...");
  Serial.println("-----------------------------------------------------------");

  for (int i = 0; i < NUM_CHANNELS; i++) {
    channels[i].sensor.sample();

    float r = channels[i].sensor.getResistance();
    float tLinear = channels[i].sensor.getTemperature();
    float tCvd = calculateCvdTemperature(r);
    uint8_t status = channels[i].sensor.getStatus();

    Serial.print("[Pin ");
    Serial.print(channels[i].csPin);
    Serial.print("] ");
    Serial.print(channels[i].name);
    Serial.print(" | R: ");
    Serial.print(r, 2);
    Serial.print(" Ohm | CVD Temp: ");
    if (isnan(tCvd)) {
      Serial.print("NAN");
    } else {
      Serial.print(tCvd, 2);
      Serial.print(" C");
    }
    Serial.print(" (Linear: ");
    Serial.print(tLinear, 2);
    Serial.print(" C) | Status: ");
    printFaultDetails(status);
    Serial.println();

    delay(20);
  }

  Serial.println("-----------------------------------------------------------");
  delay(2000);
}
