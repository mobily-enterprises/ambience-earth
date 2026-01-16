#pragma once

#include "app_state.h"

lv_obj_t *build_screen(ScreenId id);
void update_active_screen();
void update_screensaver(uint32_t now_ms);

void update_logs_screen();
void update_slot_summary_screen();
void update_cal_moist_screen();
void update_cal_flow_screen();

void wizard_render_step();
void time_date_render_step();
void maybe_refresh_initial_setup();
