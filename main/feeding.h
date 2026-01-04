#ifndef FEEDING_H
#define FEEDING_H

#include <stdint.h>

void feedingMenu();
void feedingTick();
bool feedingIsActive();
bool feedingIsEnabled();
void feedingSetEnabled(bool enabled);
bool feedingIsPausedForUi();
void feedingPauseForUi();
void feedingResumeAfterUi();

typedef struct {
  bool active;
  uint8_t slotIndex;
  bool pumpOn;
  bool moistureReady;
  uint8_t moisturePercent;
  bool hasMoistureTarget;
  uint8_t moistureTarget;
  bool runoffRequired;
  bool hasMinRuntime;
  uint16_t minVolumeMl;
  uint16_t maxVolumeMl;
  uint16_t elapsedSeconds;
} FeedStatus;

bool feedingGetStatus(FeedStatus *outStatus);

#endif FEEDING_H
