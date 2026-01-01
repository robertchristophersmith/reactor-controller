#include <Arduino.h>

// PIN DEFINITIONS
// Based on Config.h
#define SERIAL_BAUD 115200

// Hardware SPI Pins (Mega)
// SCK=52, MISO=50
#define PIN_SPI_SCK 52
#define PIN_SPI_MISO 50

// Chip Selects
// We must DESELECT (Set HIGH) all other devices to prevent bus contention
const int CS_PINS[] = {22, 23, 24, 25, 26, 27, 28};
const int NUM_CS = 7;

// Pin to test
#define PIN_SPI_CS_TEST 22

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial)
    delay(10);

  Serial.println("\n\n--- MAX6675/31855 DIAGNOSTIC TOOL v2 ---");
  Serial.println("Initializing ALL CS pins to HIGH (Disabled)...");

  // Initialize ALL CS pins to OUTPUT HIGH to shut them up
  for (int i = 0; i < NUM_CS; i++) {
    pinMode(CS_PINS[i], OUTPUT);
    digitalWrite(CS_PINS[i], HIGH);
  }

  Serial.print("Testing Target CS Pin: ");
  Serial.println(PIN_SPI_CS_TEST);
  Serial.print("SCK Pin: ");
  Serial.println(PIN_SPI_SCK);
  Serial.print("MISO Pin: ");
  Serial.println(PIN_SPI_MISO);

  // Configure SPI Pins
  pinMode(PIN_SPI_SCK, OUTPUT);
  pinMode(PIN_SPI_MISO, INPUT); // Ensure MISO is Input

  digitalWrite(PIN_SPI_SCK, LOW); // Idle Low
}

uint32_t spiread32() {
  uint32_t d = 0;

  digitalWrite(PIN_SPI_CS_TEST, LOW);
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

  digitalWrite(PIN_SPI_CS_TEST, HIGH);
  return d;
}

void loop() {
  Serial.println("\n----------------------------------------");

  uint32_t raw = spiread32();

  Serial.print("Raw 32-bit (Hex): 0x");
  Serial.println(raw, HEX);
  Serial.print("Raw 32-bit (Bin): ");
  for (int i = 31; i >= 0; i--) {
    Serial.print((raw >> i) & 1);
    if (i % 8 == 0)
      Serial.print(" ");
  }
  Serial.println();

  // --- INTERPRET AS MAX6675 (16-bit) ---
  uint16_t raw16 = (raw >> 16) & 0xFFFF; // Look at top 16 bits
  // Note: if 6675, bits 0-15 might be last bit repeated or Z.

  Serial.print("Assuming MAX6675 (Top 16 bits): 0x");
  Serial.println(raw16, HEX);
  if (raw16 & 0x4) {
    Serial.println("  -> MAX6675 Input Open");
  } else {
    uint16_t tempRaw = (raw16 >> 3) & 0xFFF;
    float tempC = tempRaw * 0.25;
    Serial.print("  -> MAX6675 Temp: ");
    Serial.print(tempC);
    Serial.println(" C");
  }

  // --- INTERPRET AS MAX31855 (32-bit) ---
  Serial.println("Assuming MAX31855 (Full 32 bits):");

  // Extract Data
  int32_t temp31855 = (raw >> 18) & 0x3FFF; // 14 bits
  if (temp31855 & 0x2000)
    temp31855 |= 0xFFFFC000; // Sign extend

  float tempC_31855 = temp31855 * 0.25;
  Serial.print("  -> Probe Temp: ");
  Serial.print(tempC_31855);
  Serial.println(" C");

  int32_t internalRaw = (raw >> 4) & 0xFFF; // 12 bits
  if (internalRaw & 0x800)
    internalRaw |= 0xFFFFF000;
  float internalC = internalRaw * 0.0625;
  Serial.print("  -> Internal Temp: ");
  Serial.print(internalC);
  Serial.println(" C");

  if (raw & 0x7) {
    Serial.print("  -> FAULTS: ");
    if (raw & 0x1)
      Serial.print("[Open Circuit] ");
    if (raw & 0x2)
      Serial.print("[Short GND] ");
    if (raw & 0x4)
      Serial.print("[Short VCC] ");
    Serial.println();
  }

  delay(2000);
}
