#include <Arduino.h>

// PIN DEFINITIONS
// Based on Config.h
#define SERIAL_BAUD 115200

// Hardware SPI Pins (Mega)
// SCK=52, MISO=50
#define PIN_SPI_SCK 52
#define PIN_SPI_MISO 50

// Chip Select to Test
// Pin 22 - Gas Preheat (User suspects this is MAX31855 or MAX6675)
#define PIN_SPI_CS_TEST 22

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial)
    delay(10);

  Serial.println("\n\n--- MAX6675/31855 DIAGNOSTIC TOOL ---");
  Serial.print("Testing CS Pin: ");
  Serial.println(PIN_SPI_CS_TEST);
  Serial.print("SCK Pin: ");
  Serial.println(PIN_SPI_SCK);
  Serial.print("MISO Pin: ");
  Serial.println(PIN_SPI_MISO);

  // Configure Pins manually for bit-banging
  pinMode(PIN_SPI_CS_TEST, OUTPUT);
  pinMode(PIN_SPI_SCK, OUTPUT);
  pinMode(PIN_SPI_MISO, INPUT);

  digitalWrite(PIN_SPI_CS_TEST, HIGH); // Deselect
  digitalWrite(PIN_SPI_SCK, LOW);      // Idle Low
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
  // Frame: [Dummy][12-bit Temp][OC][ID][...unused 16 bits...]
  // Wait, standard SPI read is MSB first.
  // MAX6675 sends 16 bits. Our 32-bit read will have data in top 16 bits if
  // chip only sends 16. But if chip tri-states after 16, lower 16 bits might be
  // garbage or last bit state.

  uint16_t raw16 = (raw >> 16) & 0xFFFF;
  Serial.print("Assuming MAX6675 (Top 16 bits): 0x");
  Serial.println(raw16, HEX);

  if (raw16 & 0x4) {
    Serial.println("  -> MAX6675 Input Open (Thermocouple Broken)");
  } else {
    uint16_t tempRaw = (raw16 >> 3) & 0xFFF;
    float tempC = tempRaw * 0.25;
    Serial.print("  -> MAX6675 Temp: ");
    Serial.print(tempC);
    Serial.println(" C");
  }

  // --- INTERPRET AS MAX31855 (32-bit) ---
  // Frame: [Sign][13-bit Temp][Reserved][Fault][Res][IntSign][11-bit
  // Int][Res][SCV][SCG][OC]
  Serial.println("Assuming MAX31855 (Full 32 bits):");

  // Extract Data
  int32_t temp31855 = (raw >> 18) & 0x3FFF; // 14 bits (sign + 13 data)
  // Sign extend if needed (if 14th bit is 1)
  if (temp31855 & 0x2000)
    temp31855 |= 0xFFFFC000;

  float tempC_31855 = temp31855 * 0.25;
  Serial.print("  -> MAX31855 Probe Temp: ");
  Serial.print(tempC_31855);
  Serial.println(" C");

  if (raw & 0x7) {
    Serial.print("  -> FAULTS DETECTED: ");
    if (raw & 0x1)
      Serial.print("[Open Circuit] ");
    if (raw & 0x2)
      Serial.print("[Short to GND] ");
    if (raw & 0x4)
      Serial.print("[Short to VCC] ");
    Serial.println();
  }

  delay(2000);
}
