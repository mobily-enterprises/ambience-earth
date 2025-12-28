#include <Arduino.h>

static const uint8_t kAnalogPin = A0;

void setup() {
  Serial.begin(115200);
}

void loop() {
  static unsigned long lastPrint = 0;
  static int lastReading = -1;

  const int reading = analogRead(kAnalogPin);

  if (millis() - lastPrint >= 200 || reading != lastReading) {
    lastPrint = millis();
    lastReading = reading;
    Serial.print("A0=");
    Serial.println(reading);
  }
}
