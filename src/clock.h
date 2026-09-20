/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_CLOCK_H_
#define TC001_CLOCK_H_

#include <stdbool.h>
#include <stdint.h>

#include "timeconv.h"

/**
 * Start from the time in the battery-backed RTC, if it holds a plausible one, so the clock
 * works before (or without) the network. Call once, early.
 */
void clock_init(void);

/** Ask an NTP server for the time, and keep it in the RTC. Returns 0 on success. */
int clock_sync(void);

/** Whether any source has set the time yet. */
bool clock_is_set(void);

/** Offset of local time from UTC in seconds, including daylight saving. */
void clock_set_utc_offset(int32_t seconds);

/** The local time. Returns false, leaving @p t alone, if the time is not known yet. */
bool clock_local(struct civil_time *t);

#endif /* TC001_CLOCK_H_ */
