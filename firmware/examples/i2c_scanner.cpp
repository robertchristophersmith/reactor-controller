#include <Arduino.h>
#include <Wire.h>

void setup() {
  Wire.begin();

  // Set timeout to 3000us (3ms) and reset_on_timeout=true to prevent hanging
  // on a shorted or stuck bus.
  Wire.setWireTimeout(3000, true);

  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("\nI2C Scanner");

  // Print status of SDA/SCL pins if possible (debugging help)
  Serial.print("SDA Pin State: ");
  Serial.println(digitalRead(20)); // Mega SDA
  Serial.print("SCL Pin State: ");
  Serial.println(digitalRead(21)); // Mega SCL
}

void loop() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");

      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");

  delay(5000);
}
