#include "weightSensor.h"

// Minimal HX711 bit-bang to keep footprint tiny.

namespace {
uint8_t doutPin = WEIGHT_DOUT_PIN;
uint8_t sckPin = WEIGHT_SCK_PIN;
int32_t offsetCounts = 0;
float unitsPerCount = 1.0f;
bool lastReady = false;
float lastUnits = 0.0f;

bool waitReady(unsigned long timeoutMs = 100) {
  unsigned long start = millis();
  while (digitalRead(doutPin) == HIGH) {
    if (millis() - start >= timeoutMs) return false;
  }
  return true;
}

int32_t readRawOnce() {
  if (!waitReady()) return 0;
  uint32_t value = 0;
  for (uint8_t i = 0; i < 24; i++) {
    digitalWrite(sckPin, HIGH);
    value = (value << 1) | (digitalRead(doutPin) & 0x01);
    digitalWrite(sckPin, LOW);
  }
  // Gain 128 (channel A): one extra pulse
  digitalWrite(sckPin, HIGH);
  digitalWrite(sckPin, LOW);

  // Sign-extend 24-bit to 32-bit
  if (value & 0x800000) value |= 0xFF000000;
  return static_cast<int32_t>(value);
}
}  // namespace

void initWeightSensor() {
  pinMode(doutPin, INPUT);
  pinMode(sckPin, OUTPUT);
  digitalWrite(sckPin, LOW);
}

void weightSensorTare(uint8_t samples) {
  int64_t sum = 0;
  uint8_t count = samples ? samples : 1;
  for (uint8_t i = 0; i < count; i++) {
    sum += readRawOnce();
  }
  offsetCounts = static_cast<int32_t>(sum / count);
}

void weightSensorSetScale(float upc) {
  unitsPerCount = upc;
}

bool weightSensorRead(float *outUnits, uint8_t samples) {
  if (!outUnits) return false;
  uint8_t count = samples ? samples : 1;
  int64_t sum = 0;
  uint8_t got = 0;
  for (uint8_t i = 0; i < count; i++) {
    if (!waitReady()) break;
    sum += readRawOnce();
    got++;
  }
  if (got == 0) return false;
  int32_t avg = static_cast<int32_t>(sum / got);
  int32_t net = avg - offsetCounts;
  *outUnits = net * unitsPerCount;
  return true;
}

void weightSensorPoll() {
  float v = 0.0f;
  if (weightSensorRead(&v, 2)) {
    lastReady = true;
    lastUnits = v;
  }
}

bool weightSensorReady() {
  return lastReady;
}

float weightSensorLastValue() {
  return lastUnits;
}
