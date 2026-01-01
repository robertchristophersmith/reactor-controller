#include <Arduino.h>

// Define pins - USER: ENSURE THESE MATCH YOUR WIRING
#define PIN_SDA 20
#define PIN_SCL 21

void delayUS(int us) { delayMicroseconds(us); }

void i2c_start() {
  digitalWrite(PIN_SDA, HIGH);
  digitalWrite(PIN_SCL, HIGH);
  delayUS(10);
  digitalWrite(PIN_SDA, LOW);
  delayUS(10);
  digitalWrite(PIN_SCL, LOW);
  delayUS(10);
}

void i2c_stop() {
  digitalWrite(PIN_SCL, LOW);
  digitalWrite(PIN_SDA, LOW);
  delayUS(10);
  digitalWrite(PIN_SCL, HIGH);
  delayUS(10);
  digitalWrite(PIN_SDA, HIGH);
  delayUS(10);
}

bool i2c_writeRequest(uint8_t addr) {
  // Send Start
  i2c_start();

  uint8_t byteToSend = (addr << 1) | 0; // Write mode

  // Send 8 bits
  for (int i = 0; i < 8; i++) {
    digitalWrite(PIN_SCL, LOW);
    if (byteToSend & 0x80) {
      digitalWrite(PIN_SDA, HIGH);
    } else {
      digitalWrite(PIN_SDA, LOW);
    }
    byteToSend <<= 1;
    delayUS(10);
    digitalWrite(PIN_SCL, HIGH); // Clock High (Data valid)
    delayUS(20);                 // Stretch
  }

  // Read ACK
  digitalWrite(PIN_SCL, LOW);
  pinMode(PIN_SDA, INPUT); // Release SDA
  delayUS(10);
  digitalWrite(PIN_SCL, HIGH); // Clock High
  delayUS(10);

  bool ack = (digitalRead(PIN_SDA) == LOW); // LOW is ACK

  digitalWrite(PIN_SCL, LOW);
  pinMode(PIN_SDA, OUTPUT);   // Re-drive SDA
  digitalWrite(PIN_SDA, LOW); // Safe state

  i2c_stop();
  return ack;
}

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;
  delay(1000);
  Serial.println("\n=== Bit-Bang I2C Scanner ===");

  pinMode(PIN_SDA, OUTPUT);
  pinMode(PIN_SCL, OUTPUT);

  // Idle state
  digitalWrite(PIN_SDA, HIGH);
  digitalWrite(PIN_SCL, HIGH);
}

void loop() {
  Serial.println("Scanning 0x01 to 0x7F...");
  int found = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    if (i2c_writeRequest(addr)) {
      Serial.print("Device found at 0x");
      Serial.println(addr, HEX);
      found++;
    }
  }

  if (found == 0)
    Serial.println("No devices found.");

  delay(3000);
}
