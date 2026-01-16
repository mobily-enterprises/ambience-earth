#pragma once

#include "app_state.h"

void sim_init();
void sim_tick();
void sim_start_feed(int slot_index);

/*
 * sim_factory_reset
 * Restores defaults, clears logs, and resets simulation state.
 * Example:
 *   sim_factory_reset();
 */
void sim_factory_reset();
