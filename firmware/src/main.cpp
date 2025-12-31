#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

// Config
#define PIN_SPI_SCK 52
#define PIN_SPI_MISO 50
#define PIN_SPI_MOSI 51

// CS Pins to test
const int csPins[] = {22, 23, 24, 25, 26, 27, 28};
const int numCsPins = 7;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10);
  Serial.println("\n\n=== ARDUINO HARDWARE DIAGNOSTIC ===");
  Serial.println("1. SPI Loopback Test (Requires Jumper Pin 50 <-> Pin 51)");
  Serial.println("2. I2C Bus Scan");
  Serial.println("3. Toggle CS Pins (Check for stuck pins)");
  Serial.println("=======================================");

  // --- SPI TEST ---
  Serial.println("\n[TEST] SPI Loopback...");
  pinMode(22, OUTPUT); // Set at least one CS
  digitalWrite(22, HIGH);

  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV16); // Moderate speed

  byte testByte = 0xAB; // Pattern 10101011
  byte received = SPI.transfer(testByte);

  Serial.print("Sent: 0x");
  Serial.print(testByte, HEX);
  Serial.print(" Received: 0x");
  Serial.println(received, HEX);

  if (received == testByte) {
    Serial.println("RESULT: PASS - SPI Port is healthy.");
  } else if (received == 0x00) {
    Serial.println("RESULT: FAIL - Received 0x00. MISO is stuck LOW or "
                   "loopback wire missing.");
  } else if (received == 0xFF) {
    Serial.println("RESULT: FAIL - Received 0xFF. MISO is stuck HIGH.");
  } else {
    Serial.println("RESULT: FAIL - Data mismatch (Noise/Speed issue?).");
  }

  // --- I2C TEST ---
  Serial.println("\n[TEST] I2C Bus Scan...");
  Wire.begin();
  int nDevices = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C Device found at 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found.");
  else
    Serial.println("I2C Scan Complete.");

  // --- CS PIN TEST ---
  Serial.println("\n[TEST] Toggling CS Pins 22-28 (Active LOW)...");
  Serial.println("Pins will toggle every 100ms. Check voltages.");
  for (int i = 0; i < numCsPins; i++) {
    pinMode(csPins[i], OUTPUT);
    digitalWrite(csPins[i], HIGH);
  }
}

void loop() {
  // Continuously toggle CS pins so user can test with multimeter/LED
  for (int i = 0; i < numCsPins; i++) {
    digitalWrite(csPins[i], LOW);
  }
  delay(100);
  for (int i = 0; i < numCsPins; i++) {
    digitalWrite(csPins[i], HIGH);
  }
  delay(100);

  // Continuous SPI test
  byte test = 0x55;
  byte rx = SPI.transfer(test);
  if (rx != test) {
    // Only print on failure to avoid spam, but print occasionally to show it's
    // alive
  }
  static int count = 0;
  if (count++ > 20) {
    Serial.print("."); // Heartbeat
    count = 0;
  }
}
