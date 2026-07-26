#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_MAX31855.h>

// Standard Mega Hardware SPI Pins
#define PIN_SCK 52
#define PIN_MISO 50
#define PIN_CS_GAS_EXT 26

// Alternative Pins for testing if Mega Pin 50/52 was damaged
#define ALT_PIN_SCK 7
#define ALT_PIN_MISO 5

Adafruit_MAX31855 hwTC(PIN_CS_GAS_EXT, &SPI);
Adafruit_MAX31855 swTC_Std(PIN_SCK, PIN_CS_GAS_EXT, PIN_MISO);
Adafruit_MAX31855 swTC_Alt(ALT_PIN_SCK, PIN_CS_GAS_EXT, ALT_PIN_MISO);

uint32_t rawBitBangRead(uint8_t sclk, uint8_t cs, uint8_t miso) {
  uint32_t d = 0;
  digitalWrite(cs, LOW);
  delayMicroseconds(10);
  for (int i = 31; i >= 0; i--) {
    digitalWrite(sclk, LOW);
    delayMicroseconds(10);
    d <<= 1;
    if (digitalRead(miso)) {
      d |= 1;
    }
    digitalWrite(sclk, HIGH);
    delayMicroseconds(10);
  }
  digitalWrite(cs, HIGH);
  return d;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  Serial.println("\n==================================================");
  Serial.println("  MAX31855 Advanced Pin & SPI Payload Scanner    ");
  Serial.println("==================================================");

  // Hardware SS Pin 53 must be OUTPUT HIGH on ATmega2560
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);

  pinMode(PIN_CS_GAS_EXT, OUTPUT);
  digitalWrite(PIN_CS_GAS_EXT, HIGH);

  pinMode(PIN_SCK, OUTPUT);
  pinMode(PIN_MISO, INPUT);
  pinMode(ALT_PIN_SCK, OUTPUT);
  pinMode(ALT_PIN_MISO, INPUT);

  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV32);

  hwTC.begin();
  swTC_Std.begin();
  swTC_Alt.begin();
}

void loop() {
  Serial.println("\n--- Live Diagnostic Read (CS Pin 26) ---");

  // 1. Raw Bit-Bang Read on Standard Pins 52 (CLK) and 50 (MISO)
  uint32_t rawStd = rawBitBangRead(PIN_SCK, PIN_CS_GAS_EXT, PIN_MISO);
  Serial.print("Raw 32-bit payload on Pins 52 (CLK) & 50 (MISO): 0x");
  Serial.println(rawStd, HEX);

  // 2. Raw Bit-Bang Read on ALT Pins 7 (CLK) and 5 (MISO)
  uint32_t rawAlt = rawBitBangRead(ALT_PIN_SCK, PIN_CS_GAS_EXT, ALT_PIN_MISO);
  Serial.print("Raw 32-bit payload on ALT Pins 7 (CLK) & 5 (MISO): 0x");
  Serial.println(rawAlt, HEX);

  // 3. Adafruit library reads
  float hwTemp = hwTC.readCelsius();
  uint8_t hwErr = hwTC.readError();
  Serial.print("[Hardware SPI] Temp: ");
  if (isnan(hwTemp)) Serial.print("NAN"); else Serial.print(hwTemp, 2);
  Serial.print(" °C | Error Code: 0x"); Serial.println(hwErr, HEX);

  float swAltTemp = swTC_Alt.readCelsius();
  uint8_t swAltErr = swTC_Alt.readError();
  Serial.print("[ALT Software SPI (Pins 7&5)] Temp: ");
  if (isnan(swAltTemp)) Serial.print("NAN"); else Serial.print(swAltTemp, 2);
  Serial.print(" °C | Error Code: 0x"); Serial.println(swAltErr, HEX);

  // 4. Root Cause Analysis
  if (rawStd == 0xFFFFFFFF && rawAlt == 0xFFFFFFFF) {
    Serial.println("\n>> RESULT: 0xFFFFFFFF (All HIGH) on BOTH standard and ALT pins!");
    Serial.println("   [A] CS Pin 26 is NOT reaching the MAX31855 board CS terminal (check wire or pin).");
    Serial.println("   [B] MAX31855 Board VIN (5V/3.3V) or GND power is disconnected/off.");
    Serial.println("   [C] MISO wire (DO) is not connected or the MAX31855 chip itself is unpowered.");
  } else if (rawStd == 0x00000000 && rawAlt == 0x00000000) {
    Serial.println("\n>> RESULT: 0x00000000 (All LOW)!");
    Serial.println("   [A] MISO line (DO) is shorted directly to GND.");
  } else if (rawStd == 0xFFFFFFFF && rawAlt != 0xFFFFFFFF && !isnan(swAltTemp)) {
    Serial.println("\n>> RESULT: ALT Pins 7 & 5 WORK PERFECTLY!");
    Serial.println("   Mega Pin 50 (MISO) or Pin 52 (CLK) was damaged by the previous shorted module.");
    Serial.println("   -> We can switch the firmware to use Software SPI on Pins 7 & 5!");
  } else {
    Serial.print("\n>> RESULT: Raw Payload = 0x"); Serial.println(rawStd, HEX);
    if (rawStd & 0x01) Serial.println("   [!] OPEN CIRCUIT: Thermocouple yellow/red wires missing or loose in screw terminal.");
    if (rawStd & 0x02) Serial.println("   [!] SHORT TO GND: Thermocouple metal tip touching grounded pipe/frame.");
  }

  delay(3000);
}
