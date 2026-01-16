#pragma once

#include "app_state.h"

void add_log(const LogEntry &entry);
LogEntry build_value_log();
LogEntry build_boot_log();
LogEntry build_feed_log();
