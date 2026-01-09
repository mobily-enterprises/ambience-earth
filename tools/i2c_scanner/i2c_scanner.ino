
  #include <Wire.h>

  void setup() {
    Wire.begin();
    Serial.begin(9600);
    while (!Serial) {}
    Serial.println("I2C scan");
  }

  void loop() {
    byte count = 0;
    for (byte addr = 3; addr < 120; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        Serial.print("Found 0x");
        if (addr < 16) Serial.print('0');
        Serial.println(addr, HEX);
        count++;
      }
    }
    if (!count) Serial.println("No devices");
    delay(2000);
  }

