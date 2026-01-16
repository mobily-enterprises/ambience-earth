#pragma once

#include <stdint.h>

/*
 * feedingTick
 * Advances the feeding state machine (start/stop logic, pump pulses).
 * Example:
 *   feedingTick();
 */
void feedingTick();

/*
 * feedingIsActive
 * Returns true if a feed session is currently running.
 * Example:
 *   if (feedingIsActive()) { ... }
 */
bool feedingIsActive();

/*
 * feedingRunoffWarning
 * Returns true if the last feed had a runoff warning.
 * Example:
 *   if (feedingRunoffWarning()) { ... }
 */
bool feedingRunoffWarning();

/*
 * feedingClearRunoffWarning
 * Clears any stored runoff warning flag.
 * Example:
 *   feedingClearRunoffWarning();
 */
void feedingClearRunoffWarning();

/*
 * feedingIsEnabled
 * Returns true if feeding is enabled (not paused).
 * Example:
 *   if (!feedingIsEnabled()) { ... }
 */
bool feedingIsEnabled();

/*
 * feedingSetEnabled
 * Enables or disables feeding and persists the setting.
 * Example:
 *   feedingSetEnabled(false);
 */
void feedingSetEnabled(bool enabled);

/*
 * feedingIsPausedForUi
 * Returns true if feeding is paused due to UI navigation.
 * Example:
 *   if (feedingIsPausedForUi()) { ... }
 */
bool feedingIsPausedForUi();

/*
 * feedingPauseForUi
 * Pauses feeding while a UI flow is active.
 * Example:
 *   feedingPauseForUi();
 */
void feedingPauseForUi();

/*
 * feedingResumeAfterUi
 * Resumes feeding after leaving a UI flow.
 * Example:
 *   feedingResumeAfterUi();
 */
void feedingResumeAfterUi();

/*
 * feedingForceFeed
 * Starts an immediate feed for the given slot.
 * Example:
 *   feedingForceFeed(0);
 */
void feedingForceFeed(uint8_t slotIndex);

/*
 * feedingBaselineInit
 * Loads baseline-related state from recent logs.
 * Example:
 *   feedingBaselineInit();
 */
void feedingBaselineInit();

/*
 * feedingBaselineTick
 * Updates baseline tracking after runoff-setter feeds.
 * Example:
 *   feedingBaselineTick();
 */
void feedingBaselineTick();

/*
 * feedingGetBaselinePercent
 * Returns the current baseline percent if available.
 * Example:
 *   uint8_t pct = 0;
 *   if (feedingGetBaselinePercent(&pct)) { ... }
 */
bool feedingGetBaselinePercent(uint8_t *outPercent);

/*
 * feedingHasBaselineSetter
 * Returns true if a baseline setter feed slot exists.
 * Example:
 *   if (feedingHasBaselineSetter()) { ... }
 */
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

/*
 * feedingGetStatus
 * Fills a FeedStatus snapshot for the active feed session.
 * Example:
 *   FeedStatus status = {};
 *   if (feedingGetStatus(&status)) { ... }
 */
bool feedingGetStatus(FeedStatus *outStatus);
