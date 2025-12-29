#ifndef FEEDING_H
#define FEEDING_H

#include <stdint.h>

void feedingMenu();
void feedingTick();
bool feedingIsActive();

typedef struct {
  bool active;
  uint8_t slotIndex;
  bool pumpOn;
  bool pulsed;
  bool moistureReady;
  uint8_t moisturePercent;
  bool hasMoistureTarget;
  uint8_t moistureTarget;
  bool runoffRequired;
  bool hasMinRuntime;
  uint16_t minRuntimeSeconds;
  uint16_t maxRuntimeSeconds;
  uint16_t elapsedSeconds;
} FeedStatus;

bool feedingGetStatus(FeedStatus *outStatus);

#endif FEEDING_H
