/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_TIMECONV_H_
#define TC001_TIMECONV_H_

#include <stdint.h>

/* A calendar date and time of day, in whatever time zone the caller has in mind */
struct civil_time {
	int year;   /* e.g. 2026 */
	int month;  /* 1 to 12 */
	int day;    /* 1 to 31 */
	int hour;   /* 0 to 23 */
	int minute; /* 0 to 59 */
	int second; /* 0 to 59 */
};

/** Seconds since 1970-01-01 00:00:00 for a civil time interpreted as UTC. */
int64_t timeconv_to_unix(const struct civil_time *t);

/** The civil time for @p unix seconds since the epoch. */
void timeconv_from_unix(int64_t unix, struct civil_time *t);

#endif /* TC001_TIMECONV_H_ */
