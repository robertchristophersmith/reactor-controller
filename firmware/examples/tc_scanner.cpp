#include <Arduino.h>
#include <Adafruit_MAX31855.h>

// Common Software SPI Pins (Bypassing damaged Mega Pin 50/52)
#define PIN_SCK 7
#define PIN_MISO 5

// Chip Select (CS) Pins for all 5 Thermocouple Breakout Boards
struct TcChannel {
  const char *name;
  uint8_t csPin;
};

TcChannel channels[] = {
    {"Electronics Housing", 36},
    {"Feedstock Reservoir", 38},
    {"Feedstock Preheater", 40},
    {"Liquid Reactor    ", 42},
    {"Gas Reactor (Int) ", 44},
    {"Gas Reactor (Ext) ", 46}
};

const int NUM_CHANNELS = sizeof(channels) / sizeof(channels[0]);

uint32_t rawBitBangRead(uint8_t sclk, uint8_t cs, uint8_t miso) {
  uint32_t d = 0;

  // Deselect all CS pins first to prevent SPI bus contention
  for (int i = 0; i < NUM_CHANNELS; i++) {
    digitalWrite(channels[i].csPin, HIGH);
  }

  // Select target CS
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

void analyzePayload(const char *name, uint8_t csPin, uint32_t payload) {
  Serial.print("[Pin ");
  if (csPin < 10) Serial.print(" ");
  Serial.print(csPin);
  Serial.print("] ");
  Serial.print(name);
  Serial.print(" | Hex: 0x");

  // Print 8 hex digits with leading zeros
  for (int i = 7; i >= 0; i--) {
    uint8_t nibble = (payload >> (i * 4)) & 0x0F;
    Serial.print(nibble, HEX);
  }

  Serial.print(" -> ");

  if (payload == 0xFFFFFFFF) {
    Serial.println("STATUS: [FAIL] 0xFFFFFFFF (NO SPI RESPONSE - Check 5V/GND power, CS wire, or DO wire)");
    return;
  }

  if (payload == 0x00000000) {
    Serial.println("STATUS: [FAIL] 0x00000000 (ALL LOW - MISO line shorted to GND or another board holding bus LOW)");
    return;
  }

  // Extract error bits (Bits 0, 1, 2)
  uint8_t err = payload & 0x07;
  bool faultBit = (payload >> 16) & 0x01;

  // Extract internal temp (Bits 15..4)
  int16_t rawInt = (payload >> 4) & 0x07FF;
  if ((payload >> 4) & 0x0800) {
    rawInt |= 0xF800; // Sign extend negative
  }
  float internalC = rawInt * 0.0625;

  // Extract TC temp (Bits 31..18)
  int32_t rawTc = payload;
  if (rawTc & 0x80000000) {
    rawTc = 0xFFFFC000 | ((rawTc >> 18) & 0x3FFF);
  } else {
    rawTc >>= 18;
  }
  float tcC = rawTc * 0.25;

  if (err == 0 && !faultBit) {
    Serial.print("STATUS: [OK] Temp: ");
    Serial.print(tcC, 2);
    Serial.print(" °C (Internal: ");
    Serial.print(internalC, 2);
    Serial.println(" °C)");
  } else {
    Serial.print("STATUS: [FAULT] ");
    if (err & 0x01) Serial.print("OPEN CIRCUIT (Wires missing or loose in terminal block) ");
    if (err & 0x02) Serial.print("SHORT TO GND (Probe metal tip touching grounded reactor body) ");
    if (err & 0x04) Serial.print("SHORT TO VCC (Probe wire touching power line) ");
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  Serial.println("\n=======================================================");
  Serial.println("   MAX31855 Full 6-Channel Diagnostic Scanner Tool    ");
  Serial.println("   Software SPI: CLK = Pin 7  |  DO (MISO) = Pin 5    ");
  Serial.println("=======================================================");

  // Set SCK and MISO pin modes
  pinMode(PIN_SCK, OUTPUT);
  pinMode(PIN_MISO, INPUT);

  // Set all CS pins to OUTPUT HIGH
  for (int i = 0; i < NUM_CHANNELS; i++) {
    pinMode(channels[i].csPin, OUTPUT);
    digitalWrite(channels[i].csPin, HIGH);
  }

  // ATmega2560 SS Pin 53 must be OUTPUT HIGH
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);
}

void loop() {
  Serial.println("\n-------------------------------------------------------");
  Serial.println("  Scanning All 6 MAX31855 Breakout Channels...");
  Serial.println("-------------------------------------------------------");

  for (int i = 0; i < NUM_CHANNELS; i++) {
    uint32_t payload = rawBitBangRead(PIN_SCK, channels[i].csPin, PIN_MISO);
    analyzePayload(channels[i].name, channels[i].csPin, payload);
    delay(50);
  }

  Serial.println("-------------------------------------------------------");
  delay(3000);
}
