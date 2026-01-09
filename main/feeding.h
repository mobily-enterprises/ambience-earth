#ifndef FEEDING_H
#define FEEDING_H

#include <stdint.h>

void feedingMenu();
void feedingTick();
bool feedingIsActive();
bool feedingRunoffWarning();
void feedingClearRunoffWarning();
bool feedingIsEnabled();
void feedingSetEnabled(bool enabled);
bool feedingIsPausedForUi();
void feedingPauseForUi();
void feedingResumeAfterUi();
void feedingForceFeed(uint8_t slotIndex);
bool feedingForceFeedMenu();
void feedingBaselineInit();
void feedingBaselineTick();
bool feedingGetBaselinePercent(uint8_t *outPercent);
bool feedingHasBaselineSetter();

typedef struct {
  bool active;
  uint8_t slotIndex;
  bool pumpOn;
  bool moistureReady;
  uint8_t moisturePercent;
  bool hasMoistureTarget;
  uint8_t moistureTarget;
  bool runoffRequired;
  uint16_t maxVolumeMl;
  uint16_t elapsedSeconds;
} FeedStatus;

bool feedingGetStatus(FeedStatus *outStatus);
void editMaxDailyWater();

#endif FEEDING_H
