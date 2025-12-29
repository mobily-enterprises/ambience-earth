#include "moistureSensor.h"

#include <Arduino.h>
#include "config.h"

extern Config config;

enum ReadMode : uint8_t {
  READ_MODE_LAZY = 0,
  READ_MODE_REALTIME = 1
};

enum LazyState : uint8_t {
  LAZY_STATE_IDLE = 0,
  LAZY_STATE_WARMING,
  LAZY_STATE_SAMPLING
};

static uint16_t lazyValue = 0;
static uint16_t realtimeRaw = 0;
static float realtimeAvg = 0.0f;
static ReadMode readMode = READ_MODE_LAZY;
static LazyState lazyState = LAZY_STATE_IDLE;
static bool sensorPowered = false;

static unsigned long sensorOnTime = 0;
static unsigned long windowStartAt = 0;
static unsigned long nextSampleAt = 0;
static unsigned long nextWindowAt = 0;
static unsigned long realtimeWarmupUntil = 0;
static unsigned long realtimeLastSampleAt = 0;
static unsigned long realtimeRiseStartAt = 0;
static bool realtimeRiseActive = false;
static unsigned long realtimeFallStartAt = 0;
static bool realtimeFallActive = false;
static bool realtimeSeeded = false;

static uint32_t windowSum = 0;
static uint32_t windowPercentSum = 0;
static uint16_t windowCount = 0;
static uint8_t windowMinPercent = 0;
static uint8_t windowMaxPercent = 0;
static uint8_t windowMinHold = 0;
static uint8_t windowMaxHold = 0;
static bool windowMinConfirmed = false;
static bool windowMaxConfirmed = false;

static void sensorPowerOn() {
  if (sensorPowered) return;
  digitalWrite(SOIL_MOISTURE_SENSOR_POWER, HIGH);
  sensorPowered = true;
}

static void sensorPowerOff() {
  if (!sensorPowered) return;
  digitalWrite(SOIL_MOISTURE_SENSOR_POWER, LOW);
  sensorPowered = false;
}

static uint16_t readMedian3() {
  uint16_t a = analogRead(SOIL_MOISTURE_SENSOR);
  uint16_t b = analogRead(SOIL_MOISTURE_SENSOR);
  uint16_t c = analogRead(SOIL_MOISTURE_SENSOR);

  uint16_t min_ab = (a < b) ? a : b;
  uint16_t max_ab = (a > b) ? a : b;

  if (c < min_ab) return min_ab;
  if (c > max_ab) return max_ab;
  return c;
}

static void resetWindowAccum() {
  windowSum = 0;
  windowPercentSum = 0;
  windowCount = 0;
  windowMinPercent = 100;
  windowMaxPercent = 0;
  windowMinHold = 0;
  windowMaxHold = 0;
  windowMinConfirmed = false;
  windowMaxConfirmed = false;
}

static void finalizeWindow() {
  if (windowCount == 0) return;

  lazyValue = (uint16_t)(windowSum / windowCount);
}

static void resetRealtimeFilter(uint16_t seed) {
  realtimeAvg = (float)seed;
  realtimeRaw = seed;
  realtimeSeeded = true;
  realtimeLastSampleAt = 0;
  realtimeWarmupUntil = millis() + SENSOR_STABILIZATION_TIME;
  realtimeRiseStartAt = 0;
  realtimeRiseActive = false;
  realtimeFallStartAt = 0;
  realtimeFallActive = false;
}

static void tickRealtime(bool forceSample) {
  if (!sensorPowered) return;

  unsigned long now = millis();
  if (now < realtimeWarmupUntil) return;

  if (!forceSample && realtimeLastSampleAt && now - realtimeLastSampleAt < SENSOR_SAMPLE_INTERVAL) {
    return;
  }

  uint16_t raw = readMedian3();
  realtimeRaw = raw;

  if (!realtimeSeeded) {
    realtimeAvg = (float)raw;
    realtimeSeeded = true;
  }

  uint8_t rawPercent = soilMoistureAsPercentage(raw);
  uint8_t avgPercent = soilMoistureAsPercentage((uint16_t)(realtimeAvg + 0.5f));
  int16_t diffPercent = (int16_t)rawPercent - (int16_t)avgPercent;

  bool aboveBand = diffPercent > (int16_t)SENSOR_FEED_DEADBAND_PERCENT;
  bool belowBand = diffPercent < -(int16_t)SENSOR_FEED_DEADBAND_PERCENT;

  if (aboveBand) {
    if (!realtimeRiseActive) {
      realtimeRiseActive = true;
      realtimeRiseStartAt = now;
    }
  } else {
    realtimeRiseActive = false;
    realtimeRiseStartAt = 0;
  }

  if (belowBand) {
    if (!realtimeFallActive) {
      realtimeFallActive = true;
      realtimeFallStartAt = now;
    }
  } else {
    realtimeFallActive = false;
    realtimeFallStartAt = 0;
  }

  bool riseOk = realtimeRiseActive && (now - realtimeRiseStartAt >= SENSOR_FEED_RISE_MS);
  bool fallOk = realtimeFallActive && (now - realtimeFallStartAt >= SENSOR_FEED_DROP_MS);
  bool shouldUpdate = false;
  float tau = SENSOR_FEED_TAU_SLOW_MS;

  if (aboveBand && riseOk) {
    shouldUpdate = true;
    tau = SENSOR_FEED_TAU_FAST_MS;
  } else if (belowBand) {
    shouldUpdate = true;
    tau = fallOk ? SENSOR_FEED_TAU_FAST_MS : SENSOR_FEED_TAU_SLOW_MS;
  }

  if (shouldUpdate) {
    unsigned long dt = realtimeLastSampleAt ? (now - realtimeLastSampleAt) : SENSOR_SAMPLE_INTERVAL;
    float alpha = (float)dt / (tau + (float)dt);
    realtimeAvg += alpha * ((float)raw - realtimeAvg);
  }

  realtimeLastSampleAt = now;
}

void initMoistureSensor() {
  pinMode(SOIL_MOISTURE_SENSOR_POWER, OUTPUT);
  pinMode(SOIL_MOISTURE_SENSOR, INPUT);

  sensorPowerOff();
  readMode = READ_MODE_LAZY;
  lazyState = LAZY_STATE_IDLE;
  nextWindowAt = millis();

  sensorPowerOn();
  delay(SENSOR_STABILIZATION_TIME);
  lazyValue = readMedian3();
  sensorPowerOff();
  realtimeRaw = lazyValue;
  realtimeAvg = (float)lazyValue;
  realtimeSeeded = true;
}

uint16_t runSoilSensorLazyReadings() {
  return soilSensorOp(0);
}

uint16_t getSoilMoisture() {
  return soilSensorOp(1);
}

void setSoilSensorLazy() {
  soilSensorOp(2);
}

void setSoilSensorLazySeed(uint16_t seed) {
  lazyValue = seed;
  soilSensorOp(2);
}

void setSoilSensorRealTime() {
  soilSensorOp(3);
}

bool soilSensorIsActive() {
  return sensorPowered;
}

bool soilSensorRealtimeReady() {
  if (readMode != READ_MODE_REALTIME) return false;
  if (!sensorPowered) return false;
  return millis() >= realtimeWarmupUntil;
}

uint16_t soilSensorGetRealtimeAvg() {
  if (!realtimeSeeded) return lazyValue;
  return (uint16_t)(realtimeAvg + 0.5f);
}

uint16_t soilSensorGetRealtimeRaw() {
  return realtimeRaw;
}

uint16_t soilSensorOp(uint8_t op) {
  switch (op) {
    case 0: { // Tick lazy state machine
      if (readMode != READ_MODE_LAZY) {
        tickRealtime(false);
        return soilSensorGetRealtimeAvg();
      }

      const unsigned long now = millis();

      switch (lazyState) {
        case LAZY_STATE_IDLE:
          if (now >= nextWindowAt) {
            sensorPowerOn();
            sensorOnTime = now;
            lazyState = LAZY_STATE_WARMING;
          }
          break;

        case LAZY_STATE_WARMING:
          if (now - sensorOnTime >= SENSOR_STABILIZATION_TIME) {
            windowStartAt = now;
            nextSampleAt = now;
            resetWindowAccum();
            lazyState = LAZY_STATE_SAMPLING;
          }
          break;

        case LAZY_STATE_SAMPLING:
          if (now >= nextSampleAt) {
            uint16_t raw = readMedian3();
            uint8_t percent = soilMoistureAsPercentage(raw);

            windowSum += raw;
            windowPercentSum += percent;
            windowCount++;

            if (percent < windowMinPercent) {
              windowMinPercent = percent;
              windowMinHold = 1;
              windowMinConfirmed = false;
            } else if (!windowMinConfirmed) {
              if (percent <= windowMinPercent + SENSOR_EXTREME_HYSTERESIS) {
                if (windowMinHold < 255) windowMinHold++;
                if (windowMinHold >= SENSOR_EXTREME_HOLD_SAMPLES) {
                  windowMinConfirmed = true;
                }
              } else {
                windowMinHold = 0;
              }
            }

            if (percent > windowMaxPercent) {
              windowMaxPercent = percent;
              windowMaxHold = 1;
              windowMaxConfirmed = false;
            } else if (!windowMaxConfirmed) {
              if (percent + SENSOR_EXTREME_HYSTERESIS >= windowMaxPercent) {
                if (windowMaxHold < 255) windowMaxHold++;
                if (windowMaxHold >= SENSOR_EXTREME_HOLD_SAMPLES) {
                  windowMaxConfirmed = true;
                }
              } else {
                windowMaxHold = 0;
              }
            }

            nextSampleAt = now + SENSOR_SAMPLE_INTERVAL;
          }

          bool rangeOk = (windowMaxPercent >= (uint8_t)(windowMinPercent + SENSOR_EXTREME_RANGE_MIN));
          bool extremesOk = windowMinConfirmed && windowMaxConfirmed && rangeOk;
          bool minWindowOk = (now - windowStartAt >= SENSOR_WINDOW_MIN_DURATION);

          if ((extremesOk && minWindowOk) || (now - windowStartAt >= SENSOR_WINDOW_DURATION)) {
            finalizeWindow();
            sensorPowerOff();
            lazyState = LAZY_STATE_IDLE;
            nextWindowAt = now + SENSOR_SLEEP_INTERVAL;
          }
          break;
      }

      return lazyValue;
    }

    case 1: { // Read (lazy value or real-time read)
      if (readMode == READ_MODE_REALTIME) {
        tickRealtime(true);
        return soilSensorGetRealtimeAvg();
      }
      return lazyValue;
    }

    case 2: { // Set lazy mode
      readMode = READ_MODE_LAZY;
      lazyState = LAZY_STATE_IDLE;
      sensorPowerOff();
      nextWindowAt = millis();
      realtimeSeeded = false;
      break;
    }

    case 3: { // Set real-time mode
      readMode = READ_MODE_REALTIME;
      lazyState = LAZY_STATE_IDLE;
      sensorPowerOn();
      resetRealtimeFilter(lazyValue);
      break;
    }

    default:
      return 0xFFFF;
  }

  return 0;
}

uint8_t soilMoistureAsPercentage(uint16_t soilMoisture) {
  int32_t dry = config.moistSensorCalibrationDry;
  int32_t soaked = config.moistSensorCalibrationSoaked;
  if (dry == soaked) return 0;

  int32_t delta;
  int32_t shifted;
  if (dry > soaked) {
    delta = dry - soaked;
    shifted = dry - soilMoisture;
  } else {
    delta = soaked - dry;
    shifted = soilMoisture - dry;
  }

  if (shifted < 0) shifted = 0;
  if (shifted > delta) shifted = delta;

  return (uint8_t)((shifted * 100) / delta);
}
