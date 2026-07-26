#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_MAX31855.h>

// Pin assignments for Mega 2560
#define PIN_SCK 52
#define PIN_MISO 50
#define PIN_CS_GAS_EXT 26

// Hardware SPI instance
Adafruit_MAX31855 hwTC(PIN_CS_GAS_EXT, &SPI);
// Software (Bit-Bang) SPI instance
Adafruit_MAX31855 swTC(PIN_SCK, PIN_CS_GAS_EXT, PIN_MISO);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  Serial.println("\n==========================================");
  Serial.println("  MAX31855 Thermocouple Diagnostic Scanner");
  Serial.println("==========================================");

  // Hardware SS Pin 53 must be OUTPUT HIGH on ATmega2560 for SPI Master mode
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);

  pinMode(PIN_CS_GAS_EXT, OUTPUT);
  digitalWrite(PIN_CS_GAS_EXT, HIGH);

  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV32);

  hwTC.begin();
  swTC.begin();
}

void loop() {
  Serial.println("\n--- Testing Pin 26 (External Gas Reactor TC) ---");

  // 1. Read Hardware SPI
  float hwTemp = hwTC.readCelsius();
  float hwInternal = hwTC.readInternal();
  uint8_t hwErr = hwTC.readError();

  Serial.print("[Hardware SPI] Temp: ");
  if (isnan(hwTemp)) Serial.print("NAN");
  else Serial.print(hwTemp, 2);
  Serial.print(" °C | Internal: ");
  Serial.print(hwInternal, 2);
  Serial.print(" °C | Error: 0x");
  Serial.println(hwErr, HEX);

  // 2. Read Software SPI
  float swTemp = swTC.readCelsius();
  float swInternal = swTC.readInternal();
  uint8_t swErr = swTC.readError();

  Serial.print("[Software SPI] Temp: ");
  if (isnan(swTemp)) Serial.print("NAN");
  else Serial.print(swTemp, 2);
  Serial.print(" °C | Internal: ");
  Serial.print(swInternal, 2);
  Serial.print(" °C | Error: 0x");
  Serial.println(swErr, HEX);

  // 3. Diagnostic output & recommendations
  uint8_t err = hwErr ? hwErr : swErr;
  if (!isnan(hwTemp) || !isnan(swTemp)) {
    Serial.println(">> RESULT: SUCCESS! Temperature reading is valid.");
  } else {
    Serial.print(">> DIAGNOSIS: ");
    if (err & 0x01) {
      Serial.println("OPEN CIRCUIT - Thermocouple probe wires are loose or disconnected in terminal block.");
    } else if (err & 0x02) {
      Serial.println("SHORT TO GND - Thermocouple metal tip is touching grounded reactor body! Insulate probe or unground sheath.");
    } else if (err & 0x04) {
      Serial.println("SHORT TO VCC - Thermocouple lead is touching VCC line.");
    } else {
      Serial.println("NO SPI RESPONSE (0xFFFFFFFF) - Check MISO (Pin 50), SCK (Pin 52), CS (Pin 26), 5V/3.3V power, or level shifter.");
    }
  }

  delay(2000);
}
