#include <Arduino.h>

static const uint8_t kModulatedPin = A0;
static const uint8_t kRawPin = A1;

void setup() {
  Serial.begin(115200);
}

void loop() {
  static unsigned long lastPrint = 0;
  static int lastRaw = -1;
  static int lastMod = -1;

  const int raw = analogRead(kRawPin);
  const int modulated = analogRead(kModulatedPin);

  if (millis() - lastPrint >= 200 || raw != lastRaw || modulated != lastMod) {
    lastPrint = millis();
    lastRaw = raw;
    lastMod = modulated;
    Serial.print("RAW=");
    Serial.print(raw);
    Serial.print(" MOD=");
    Serial.println(modulated);
  }
}
