#include <Arduino.h>

// PIN DEFINITIONS
// Based on Config.h
#define SERIAL_BAUD 115200

// Hardware SPI Pins (Mega)
// SCK=52, MISO=50
#define PIN_SPI_SCK 52
#define PIN_SPI_MISO 50

// Pins to test (22 through 28)
// 22: Gas Internal
// 23: Vaporizer Wall
// 24: Reactor Ext 1
// 25: Reactor Ext 2
// 26: Reactor Int 1
// 27: Reactor Int 2
// 28: Feedstock
const int CS_PINS[] = {22, 23, 24, 25, 26, 27, 28};
const char *CS_NAMES[] = {"22 (Gas Int)",   "23 (Vap Wall)",  "24 (Reac Ext1)",
                          "25 (Reac Ext2)", "26 (Reac Int1)", "27 (Reac Int2)",
                          "28 (Feedstock)"};
const int NUM_CS = 7;

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial)
    delay(10);

  Serial.println("\n\n--- MULTI-CHANNEL THERMOCOUPLE DIAGNOSTICS ---");
  Serial.println("Initializing ALL CS pins to HIGH (Disabled)...");

  // Initialize ALL CS pins to OUTPUT HIGH to shut them up
  for (int i = 0; i < NUM_CS; i++) {
    pinMode(CS_PINS[i], OUTPUT);
    digitalWrite(CS_PINS[i], HIGH);
  }

  // Configure SPI Pins
  pinMode(PIN_SPI_SCK, OUTPUT);
  pinMode(PIN_SPI_MISO, INPUT);   // Ensure MISO is Input
  digitalWrite(PIN_SPI_SCK, LOW); // Idle Low

  Serial.println("Setup Complete. Starting Scan Loop...");
  delay(1000);
}

// Bit-bang 32 bits from SPI
uint32_t spiread32(int csPin) {
  uint32_t d = 0;

  digitalWrite(csPin, LOW);
  delayMicroseconds(10); // Ensure CS settles

  for (int i = 31; i >= 0; i--) {
    digitalWrite(PIN_SPI_SCK, LOW);
    delayMicroseconds(10);

    // Read MISO
    if (digitalRead(PIN_SPI_MISO)) {
      d |= (1UL << i);
    }

    digitalWrite(PIN_SPI_SCK, HIGH);
    delayMicroseconds(10);
  }

  digitalWrite(csPin, HIGH);
  return d;
}

void loop() {
  Serial.println("\n========================================");
  for (int i = 0; i < NUM_CS; i++) {
    int pin = CS_PINS[i];
    Serial.print("[PIN ");
    Serial.print(CS_NAMES[i]);
    Serial.print("]... ");

    // Read Data
    uint32_t raw = spiread32(pin);

    // Print Raw Hex
    Serial.print("Hex: 0x");
    Serial.print(raw, HEX);
    Serial.print(" | ");

    // Check for "Dead" lines
    if (raw == 0x00000000) {
      Serial.println("ERROR: ALL ZEROS (GND Short / Disconnected MISO)");
      continue;
    }
    if (raw == 0xFFFFFFFF) {
      Serial.println("ERROR: ALL ONES (VCC Short / Floating)");
      continue;
    }

    // --- INTERPRET AS MAX31855 (32-bit) ---
    // Frame: [Sign 14][Temp 13][Res][Fault][Res][IntSign][Int
    // 11][Res][SCV][SCG][OC]
    int32_t temp31855 = (raw >> 18) & 0x3FFF;
    if (temp31855 & 0x2000)
      temp31855 |= 0xFFFFC000;
    float t31855 = temp31855 * 0.25;

    // Faults
    bool fault = (raw & 0x7);
    bool oc = (raw & 0x1);
    bool scg = (raw & 0x2);
    bool scv = (raw & 0x4);

    // --- INTERPRET AS MAX6675 (16-bit) ---
    // Frame: [Dummy][Temp 12][OC][ID][Trash...]
    // The MAX6675 only outputs 16 bits. If we read 32, the first 16 (MSB)
    // contain the data. The lower 16 bits will be undefined (usually the state
    // of the last bit, or Z).
    uint16_t raw16 = (raw >> 16) & 0xFFFF;
    bool oc6675 = (raw16 & 0x4);
    uint16_t tempRaw6675 = (raw16 >> 3) & 0xFFF;
    float t6675 = tempRaw6675 * 0.25;

    // --- HEURISTICS TO GUESS CHIP ---
    // MAX31855 always has bits 17 and 3 reserved as 0 ? (Not necessarily
    // reliable) MAX6675 ID bit (D1) is always 0.

    Serial.print("31855: ");
    Serial.print(t31855);
    Serial.print("C");
    if (fault) {
      Serial.print(" [Err:");
      if (oc)
        Serial.print("OPEN");
      if (scg)
        Serial.print("GND");
      if (scv)
        Serial.print("VCC");
      Serial.print("] ");
    }

    Serial.print(" / 6675: ");
    Serial.print(t6675);
    Serial.print("C");
    if (oc6675)
      Serial.print(" [OPEN]");

    Serial.println();
    delay(200);
  }
  delay(3000);
}
