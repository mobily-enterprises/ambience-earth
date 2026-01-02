#include "moistureSensor.h"

#include <Arduino.h>
#include "config.h"

extern Config config;

// Eventually, buy this: https://www.opensprinkler.com.au/product/smt50/

enum ReadMode : uint8_t {
  READ_MODE_LAZY = 0,
  READ_MODE_REALTIME = 1
};

enum LazyState : uint8_t {
  LAZY_STATE_IDLE = 0,
  LAZY_STATE_WARMING,
  LAZY_STATE_SAMPLING
};

enum WindowOwner : uint8_t {
  WINDOW_OWNER_LAZY = 0,
  WINDOW_OWNER_CALIBRATION
};

static uint16_t lazyValue = 0;
static uint16_t realtimeRaw = 0;
static int32_t realtimeAvgQ8 = 0; // Q8.8 fixed-point average
static ReadMode readMode = READ_MODE_LAZY;
static LazyState lazyState = LAZY_STATE_IDLE;
static WindowOwner windowOwner = WINDOW_OWNER_LAZY;
static bool sensorPowered = false;

static unsigned long sensorOnTime = 0;
static unsigned long windowStartAt = 0;
static unsigned long nextSampleAt = 0;
static unsigned long nextWindowAt = 0;
static unsigned long lastWindowEndAt = 0;
static unsigned long realtimeWarmupUntil = 0;
static unsigned long realtimeLastSampleAt = 0;
static bool realtimeSeeded = false;

static uint32_t windowSum = 0;
static uint16_t windowCount = 0;
static uint16_t windowMinRaw = 0;
static uint16_t windowMaxRaw = 0;
static uint16_t windowLastRaw = 0;
static bool moistureReady = false;

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
  windowCount = 0;
  windowMinRaw = 0xFFFF;
  windowMaxRaw = 0;
  windowLastRaw = 0;
}

static void finalizeWindow(SoilSensorWindowStats *out) {
  if (windowCount == 0) {
    if (out) {
      out->minRaw = 0;
      out->maxRaw = 0;
      out->avgRaw = 0;
      out->count = 0;
    }
    return;
  }

  uint16_t avg = (uint16_t)(windowSum / windowCount);
  lazyValue = avg;
  moistureReady = true;
  lastWindowEndAt = millis();
  if (out) {
    out->minRaw = windowMinRaw;
    out->maxRaw = windowMaxRaw;
    out->avgRaw = avg;
    out->count = windowCount;
  }
}

static void startWindow(WindowOwner owner) {
  windowOwner = owner;
  readMode = READ_MODE_LAZY;
  lazyState = LAZY_STATE_WARMING;
  windowStartAt = 0;
  nextSampleAt = 0;
  windowLastRaw = 0;
  sensorPowerOn();
  sensorOnTime = millis();
}

static bool tickWindow(SoilSensorWindowStats *out) {
  if (lazyState == LAZY_STATE_IDLE) return false;

  unsigned long now = millis();
  switch (lazyState) {
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
        windowLastRaw = raw;
        windowSum += raw;
        windowCount++;
        if (raw < windowMinRaw) windowMinRaw = raw;
        if (raw > windowMaxRaw) windowMaxRaw = raw;
        nextSampleAt = now + SENSOR_SAMPLE_INTERVAL;
      }

      if (now - windowStartAt >= SENSOR_WINDOW_DURATION) {
        finalizeWindow(out);
        sensorPowerOff();
        lazyState = LAZY_STATE_IDLE;
        if (windowOwner == WINDOW_OWNER_LAZY) {
          // Schedule next window relative to this window start to avoid drift
          unsigned long planned = windowStartAt + SENSOR_WINDOW_DURATION + SENSOR_SLEEP_INTERVAL;
          nextWindowAt = (planned > now) ? planned : now + SENSOR_SLEEP_INTERVAL;
        }
        return true;
      }
      break;

    default:
      break;
  }

  return false;
}

static void resetRealtimeFilter(uint16_t seed) {
  realtimeAvgQ8 = (int32_t)seed << 8;
  realtimeRaw = seed;
  realtimeSeeded = true;
  realtimeLastSampleAt = 0;
  realtimeWarmupUntil = millis() + SENSOR_STABILIZATION_TIME;
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
  moistureReady = true;

  if (!realtimeSeeded) {
    realtimeAvgQ8 = (int32_t)raw << 8;
    realtimeSeeded = true;
  }

  uint32_t dt = realtimeLastSampleAt ? (now - realtimeLastSampleAt) : SENSOR_SAMPLE_INTERVAL;
  uint32_t denom = SENSOR_FEED_TAU_SLOW_MS + dt;
  uint32_t alphaQ8 = denom ? (uint32_t)(((uint64_t)dt << 8) / denom) : 0;
  if (alphaQ8 > 256) alphaQ8 = 256;
  int32_t rawQ8 = (int32_t)raw << 8;
  int32_t deltaQ8 = rawQ8 - realtimeAvgQ8;
  realtimeAvgQ8 += (deltaQ8 * (int32_t)alphaQ8) >> 8;

  realtimeLastSampleAt = now;
}

void initMoistureSensor() {
  pinMode(SOIL_MOISTURE_SENSOR_POWER, OUTPUT);
  pinMode(SOIL_MOISTURE_SENSOR, INPUT);

  sensorPowerOff();
  readMode = READ_MODE_LAZY;
  lazyState = LAZY_STATE_IDLE;
  windowOwner = WINDOW_OWNER_LAZY;
  nextWindowAt = millis();
  resetWindowAccum();

  lazyValue = 0;
  windowLastRaw = 0;
  realtimeRaw = 0;
  realtimeAvgQ8 = 0;
  realtimeSeeded = false;
  moistureReady = false;
  lastWindowEndAt = 0;

  // Kick off a window immediately; no immediate seed sample
  startWindow(WINDOW_OWNER_LAZY);
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

bool soilSensorReady() {
  return moistureReady;
}

uint16_t soilSensorGetRealtimeAvg() {
  if (!realtimeSeeded) return lazyValue;
  return (uint16_t)((realtimeAvgQ8 + 128) >> 8);
}

uint16_t soilSensorGetRealtimeRaw() {
  return realtimeRaw;
}

void soilSensorWindowStart() {
  sensorPowerOff();
  lazyState = LAZY_STATE_IDLE;
  realtimeSeeded = false;
  startWindow(WINDOW_OWNER_CALIBRATION);
}

bool soilSensorWindowTick(SoilSensorWindowStats *out) {
  if (windowOwner != WINDOW_OWNER_CALIBRATION) return false;
  return tickWindow(out);
}

uint16_t soilSensorWindowLastRaw() {
  return windowLastRaw;
}

uint16_t soilSensorOp(uint8_t op) {
  switch (op) {
    case 0: { // Tick lazy state machine
      if (readMode != READ_MODE_LAZY) {
        tickRealtime(false);
        return soilSensorGetRealtimeAvg();
      }

      if (windowOwner != WINDOW_OWNER_LAZY) return lazyValue;

      if (lazyState == LAZY_STATE_IDLE) {
        if (millis() >= nextWindowAt) {
          startWindow(WINDOW_OWNER_LAZY);
        }
      } else {
        tickWindow(nullptr);
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
      windowOwner = WINDOW_OWNER_LAZY;
      sensorPowerOff();
      nextWindowAt = millis();
      realtimeSeeded = false;
      moistureReady = false; // force fresh window before reporting
      break;
    }

    case 3: { // Set real-time mode
      readMode = READ_MODE_REALTIME;
      lazyState = LAZY_STATE_IDLE;
      windowOwner = WINDOW_OWNER_LAZY;
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
