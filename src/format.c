/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>

#include "format.h"

void format_count(char *buf, size_t len, uint32_t value)
{
	unsigned int v = value;

	if (v < 10000) {
		snprintf(buf, len, "%u", v);
	} else if (v < 1000000) {
		snprintf(buf, len, "%uK", v / 1000);
	} else if (v < 10000000) {
		snprintf(buf, len, "%u.%uM", v / 1000000, (v % 1000000) / 100000);
	} else {
		snprintf(buf, len, "%uM", v / 1000000);
	}
}

void format_time(char *buf, size_t len, int hour, int minute, bool twelve_hour)
{
	if (!twelve_hour) {
		snprintf(buf, len, "%02d:%02d", hour, minute);
		return;
	}

	snprintf(buf, len, "%d:%02d%c", hour % 12 == 0 ? 12 : hour % 12, minute,
		 hour < 12 ? 'A' : 'P');
}

void format_date(char *buf, size_t len, int month, int day, int date_format)
{
	if (date_format == 1) { /* DATE_MONTH_DAY */
		snprintf(buf, len, "%02d/%02d", month, day);
	} else { /* DATE_DAY_MONTH */
		snprintf(buf, len, "%02d.%02d.", day, month);
	}
}

void format_temperature(char *buf, size_t len, int tenths)
{
	/* Round half away from zero so -0.4 shows as 0 and -3.5 as -4 */
	int whole = (tenths + (tenths >= 0 ? 5 : -5)) / 10;

	snprintf(buf, len, "%dC", whole);
}

void format_coordinate(char *buf, size_t len, int32_t ten_thousandths)
{
	unsigned int magnitude = ten_thousandths < 0 ? -ten_thousandths : ten_thousandths;

	snprintf(buf, len, "%s%u.%04u", ten_thousandths < 0 ? "-" : "", magnitude / 10000,
		 magnitude % 10000);
}

void format_delta(char *buf, size_t len, int64_t delta)
{
	unsigned int magnitude;
	const char *sign = delta > 0 ? "+" : (delta < 0 ? "-" : "");

	if (delta == 0) {
		snprintf(buf, len, "0");
		return;
	}

	if (delta > 999999999 || delta < -999999999) {
		magnitude = 999999999;
	} else {
		magnitude = delta < 0 ? -delta : delta;
	}

	if (magnitude < 1000) {
		snprintf(buf, len, "%s%u", sign, magnitude);
	} else if (magnitude < 1000000) {
		snprintf(buf, len, "%s%uK", sign, magnitude / 1000);
	} else {
		snprintf(buf, len, "%s%uM", sign, magnitude / 1000000);
	}
}
