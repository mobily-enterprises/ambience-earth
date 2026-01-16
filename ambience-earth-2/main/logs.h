#pragma once

#include "app_state.h"

/*
 * logs_init
 * Initializes flash-backed log storage and loads recent logs into RAM.
 * Example:
 *   logs_init();
 */
void logs_init();

/*
 * logs_wipe
 * Clears all persisted logs and resets in-memory log state.
 * Example:
 *   logs_wipe();
 */
void logs_wipe();

/*
 * add_log
 * Adds a log entry to RAM and persists it to flash storage.
 * Example:
 *   add_log(build_boot_log());
 */
void add_log(const LogEntry &entry);

/*
 * build_value_log
 * Builds a type-2 values log from the current simulated state.
 * Example:
 *   LogEntry entry = build_value_log();
 */
LogEntry build_value_log();

/*
 * build_boot_log
 * Builds a type-0 boot/marker log from the current simulated state.
 * Example:
 *   add_log(build_boot_log());
 */
LogEntry build_boot_log();

/*
 * build_feed_log
 * Builds a type-1 feed log from the current feeding session.
 * Example:
 *   LogEntry entry = build_feed_log();
 */
LogEntry build_feed_log();
