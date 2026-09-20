/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_FORMAT_H_
#define TC001_FORMAT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Format a count so it never needs more than four 3x5 glyphs (15 pixels), which leaves room
 * for a logo and an icon on either side: plain digits up to 9999, then 123K, 1.2M, 12M.
 */
void format_count(char *buf, size_t len, uint32_t value);

/** A change in a count with its sign: "+12", "-3", "0", "+1K" (thousands rounded down), "+2M". */
void format_delta(char *buf, size_t len, int64_t delta);

/** "14:05", or in 12 hour mode "2:05P" (A or P suffix, no leading zero). */
void format_time(char *buf, size_t len, int hour, int minute, bool twelve_hour);

/** A date in one of the formats of enum date_format (see config.h): "20.09." or "09/20". */
void format_date(char *buf, size_t len, int month, int day, int date_format);

/** Temperature given in tenths of a degree, rounded to whole degrees: "-3C". */
void format_temperature(char *buf, size_t len, int tenths);

/** A coordinate in ten-thousandths of a degree as decimal text: -338688 becomes "-33.8688". */
void format_coordinate(char *buf, size_t len, int32_t ten_thousandths);

#endif /* TC001_FORMAT_H_ */
