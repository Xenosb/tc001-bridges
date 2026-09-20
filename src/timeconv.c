/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "timeconv.h"

/*
 * Days since 1970-01-01 for a proleptic Gregorian date and its inverse, after Howard Hinnant's
 * public domain "chrono-compatible low-level date algorithms". They treat March as the first
 * month so the leap day falls at the end of the year.
 */
static int64_t days_from_civil(int y, int m, int d)
{
	int64_t era;
	unsigned int yoe, doy, doe;

	y -= m <= 2;
	era = (y >= 0 ? y : y - 399) / 400;
	yoe = (unsigned int)(y - era * 400);
	doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

	return era * 146097 + (int64_t)doe - 719468;
}

int64_t timeconv_to_unix(const struct civil_time *t)
{
	return days_from_civil(t->year, t->month, t->day) * 86400 + t->hour * 3600 +
	       t->minute * 60 + t->second;
}

void timeconv_from_unix(int64_t unix, struct civil_time *t)
{
	int64_t days = unix / 86400;
	int64_t rem = unix % 86400;
	int64_t era;
	unsigned int doe, yoe, doy, mp;
	int y;

	if (rem < 0) {
		rem += 86400;
		days--;
	}

	t->hour = rem / 3600;
	t->minute = rem % 3600 / 60;
	t->second = rem % 60;

	days += 719468;
	era = (days >= 0 ? days : days - 146096) / 146097;
	doe = (unsigned int)(days - era * 146097);
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	y = (int)(yoe + era * 400);
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	t->day = doy - (153 * mp + 2) / 5 + 1;
	t->month = mp < 10 ? mp + 3 : mp - 9;
	t->year = y + (t->month <= 2);
}
