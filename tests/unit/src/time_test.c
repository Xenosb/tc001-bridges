/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>

#include <zephyr/ztest.h>

#include "format.h"
#include "timeconv.h"
#include "wmo.h"

static void check_str(const char *actual, const char *expected)
{
	zassert_ok(strcmp(actual, expected), "got \"%s\", expected \"%s\"", actual, expected);
}

ZTEST(time, test_known_dates_round_trip)
{
	static const struct {
		struct civil_time t;
		int64_t unix;
	} cases[] = {
		{{1970, 1, 1, 0, 0, 0}, 0},
		{{2000, 1, 1, 0, 0, 0}, 946684800},
		{{2000, 2, 29, 12, 0, 0}, 951825600},     /* leap day in a year divisible by 400 */
		{{2026, 9, 20, 22, 15, 0}, 1789942500},   /* the time in the Open-Meteo answer I probed */
		{{2038, 1, 19, 3, 14, 8}, 2147483648},    /* past the 32 bit limit */
		{{2100, 3, 1, 0, 0, 0}, 4107542400},      /* 2100 is not a leap year */
		{{1969, 12, 31, 23, 59, 59}, -1},
	};

	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		struct civil_time back;

		zassert_equal(timeconv_to_unix(&cases[i].t), cases[i].unix, "case %u", (unsigned int)i);
		timeconv_from_unix(cases[i].unix, &back);
		zassert_equal(back.year, cases[i].t.year, "case %u", (unsigned int)i);
		zassert_equal(back.month, cases[i].t.month, "case %u", (unsigned int)i);
		zassert_equal(back.day, cases[i].t.day, "case %u", (unsigned int)i);
		zassert_equal(back.hour, cases[i].t.hour, "case %u", (unsigned int)i);
		zassert_equal(back.minute, cases[i].t.minute, "case %u", (unsigned int)i);
		zassert_equal(back.second, cases[i].t.second, "case %u", (unsigned int)i);
	}
}

ZTEST(time, test_every_day_of_several_years_round_trips)
{
	/* Walk a day at a time through leap and non-leap years, including 2100 */
	for (int64_t days = 0; days < 200 * 366; days += 1) {
		struct civil_time t, back;

		timeconv_from_unix(days * 86400 + 12345, &t);
		zassert_equal(timeconv_to_unix(&t), days * 86400 + 12345, "day %lld", (long long)days);
		zassert_true(t.month >= 1 && t.month <= 12 && t.day >= 1 && t.day <= 31);
		(void)back;
	}
}

ZTEST(time, test_time_formats)
{
	char b[12];

	format_time(b, sizeof(b), 14, 5, false);
	check_str(b, "14:05");
	format_time(b, sizeof(b), 0, 0, false);
	check_str(b, "00:00");
	format_time(b, sizeof(b), 14, 5, true);
	check_str(b, "2:05P");
	format_time(b, sizeof(b), 0, 7, true);
	check_str(b, "12:07A");
	format_time(b, sizeof(b), 12, 0, true);
	check_str(b, "12:00P");
	format_time(b, sizeof(b), 23, 59, true);
	check_str(b, "11:59P");
}

ZTEST(time, test_date_formats)
{
	char b[16];

	format_date(b, sizeof(b), 9, 20, 0);
	check_str(b, "20.09.");
	format_date(b, sizeof(b), 9, 20, 1);
	check_str(b, "09/20");
	format_date(b, sizeof(b), 1, 5, 0);
	check_str(b, "05.01.");
	format_date(b, sizeof(b), 12, 31, 1);
	check_str(b, "12/31");
}

ZTEST(time, test_temperature_and_coordinates)
{
	char b[16];

	format_temperature(b, sizeof(b), 143);
	check_str(b, "14C");
	format_temperature(b, sizeof(b), 145);
	check_str(b, "15C");
	format_temperature(b, sizeof(b), -34);
	check_str(b, "-3C");
	format_temperature(b, sizeof(b), -35);
	check_str(b, "-4C");
	format_temperature(b, sizeof(b), -4);
	check_str(b, "0C");
	format_temperature(b, sizeof(b), 0);
	check_str(b, "0C");

	format_coordinate(b, sizeof(b), 525243);
	check_str(b, "52.5243");
	format_coordinate(b, sizeof(b), -338688);
	check_str(b, "-33.8688");
	format_coordinate(b, sizeof(b), -5);
	check_str(b, "-0.0005");
	format_coordinate(b, sizeof(b), 0);
	check_str(b, "0.0000");
}

ZTEST(time, test_deltas)
{
	char b[12];

	format_delta(b, sizeof(b), 0);
	check_str(b, "0");
	format_delta(b, sizeof(b), 12);
	check_str(b, "+12");
	format_delta(b, sizeof(b), -3);
	check_str(b, "-3");
	format_delta(b, sizeof(b), 999);
	check_str(b, "+999");
	format_delta(b, sizeof(b), 1000);
	check_str(b, "+1K");
	format_delta(b, sizeof(b), -12500);
	check_str(b, "-12K");
	format_delta(b, sizeof(b), 2500000);
	check_str(b, "+2M");
}

ZTEST(time, test_weather_icons)
{
	zassert_equal(wmo_kind(0, true), WX_SUN);
	zassert_equal(wmo_kind(0, false), WX_MOON);
	zassert_equal(wmo_kind(1, true), WX_SUN);
	zassert_equal(wmo_kind(2, true), WX_PARTLY);
	zassert_equal(wmo_kind(2, false), WX_CLOUD);
	zassert_equal(wmo_kind(3, true), WX_CLOUD);
	zassert_equal(wmo_kind(45, true), WX_FOG);
	zassert_equal(wmo_kind(48, false), WX_FOG);
	zassert_equal(wmo_kind(51, true), WX_RAIN);
	zassert_equal(wmo_kind(57, true), WX_RAIN);
	zassert_equal(wmo_kind(61, true), WX_RAIN);
	zassert_equal(wmo_kind(67, true), WX_RAIN);
	zassert_equal(wmo_kind(80, true), WX_RAIN);
	zassert_equal(wmo_kind(82, false), WX_RAIN);
	zassert_equal(wmo_kind(71, true), WX_SNOW);
	zassert_equal(wmo_kind(77, true), WX_SNOW);
	zassert_equal(wmo_kind(85, true), WX_SNOW);
	zassert_equal(wmo_kind(86, false), WX_SNOW);
	zassert_equal(wmo_kind(95, true), WX_STORM);
	zassert_equal(wmo_kind(99, false), WX_STORM);
	/* Anything unknown is treated as cloud rather than showing nothing */
	zassert_equal(wmo_kind(123, true), WX_CLOUD);
	zassert_equal(wmo_kind(-1, false), WX_CLOUD);
}

ZTEST_SUITE(time, NULL, NULL, NULL, NULL, NULL);
