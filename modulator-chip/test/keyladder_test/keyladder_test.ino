#include <Arduino.h>
#include <string.h>

static const uint8_t kAnalogPin = A0;
static const uint8_t kLedPin = 13;

struct ButtonMap {
  const char *label;
  int value;
};

static const ButtonMap kButtons[] = {
  { "A", 900 },
  { "F", 700 },
  { "E", 500 },
  { "X", 300 },
  { "D", 100 },
};

static const int kTolerance = 60;

static const char *detectButton(int reading) {
  if (reading > 1000) return "NONE";
  for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); ++i) {
    if (abs(reading - kButtons[i].value) <= kTolerance) return kButtons[i].label;
  }
  return "UNKNOWN";
}

void setup() {
  Serial.begin(115200);
  pinMode(kLedPin, OUTPUT);
}

void loop() {
  static unsigned long lastPrint = 0;
  static int lastReading = -1;
  static const char *lastLabel = "";

  const int reading = analogRead(kAnalogPin);
  const char *label = detectButton(reading);

  digitalWrite(kLedPin, strcmp(label, "NONE") ? HIGH : LOW);

  if (millis() - lastPrint >= 200 || reading != lastReading || strcmp(label, lastLabel) != 0) {
    lastPrint = millis();
    lastReading = reading;
    lastLabel = label;
    Serial.print("A0=");
    Serial.print(reading);
    Serial.print(" button=");
    Serial.println(label);
  }
}
