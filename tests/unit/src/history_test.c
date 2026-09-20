/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>

#include <zephyr/ztest.h>

#include "history_core.h"

#define DAY0 20000

static struct history h;

static void before(void *unused)
{
	memset(&h, 0, sizeof(h));
}

ZTEST(history, test_empty_has_no_change)
{
	before(NULL);
	int64_t c;

	zassert_false(history_change(&h, DAY0, 0, 7, 100, &c));
}

ZTEST(history, test_change_over_seven_days)
{
	before(NULL);
	int64_t c = 0;

	for (int d = 0; d <= 10; d++) {
		zassert_true(history_record(&h, DAY0 + d, 0, 100 + d * 2));
	}
	/* Day 10 holds 120; a week earlier, day 3, held 106 */
	zassert_true(history_change(&h, DAY0 + 10, 0, 7, 120, &c));
	zassert_equal(c, 14);
	/* Not enough history for a month */
	zassert_false(history_change(&h, DAY0 + 10, 0, 30, 120, &c));
}

ZTEST(history, test_rewriting_today_updates_it_and_reports_change)
{
	before(NULL);
	zassert_true(history_record(&h, DAY0, 1, 50));
	zassert_false(history_record(&h, DAY0, 1, 50), "same value: nothing to save");
	zassert_true(history_record(&h, DAY0, 1, 51));
	zassert_equal(h.count, 1);
	zassert_equal(h.entry[0].value[1], 51);
}

ZTEST(history, test_metrics_are_independent)
{
	before(NULL);
	int64_t c;

	zassert_true(history_record(&h, DAY0, 0, 10));
	zassert_true(history_record(&h, DAY0, 2, 500));
	zassert_true(history_record(&h, DAY0 + 7, 0, 25));
	zassert_true(history_change(&h, DAY0 + 7, 0, 7, 25, &c));
	zassert_equal(c, 15);
	/* Metric 1 was never recorded */
	zassert_false(history_change(&h, DAY0 + 7, 1, 7, 5, &c));
	zassert_true(history_change(&h, DAY0 + 7, 2, 7, 520, &c));
	zassert_equal(c, 20);
}

ZTEST(history, test_shrinking_numbers_are_negative)
{
	before(NULL);
	int64_t c;

	zassert_true(history_record(&h, DAY0, 0, 1000));
	zassert_true(history_change(&h, DAY0 + 7, 0, 7, 970, &c));
	zassert_equal(c, -30);
}

ZTEST(history, test_gaps_use_the_latest_record_before_the_day_within_tolerance)
{
	before(NULL);
	int64_t c;

	/* The clock was off from day 2 to day 6 */
	zassert_true(history_record(&h, DAY0 + 1, 0, 100));
	zassert_true(history_record(&h, DAY0 + 7, 0, 130));
	/* Wanted day is 8 - 7 = 1: exact */
	zassert_true(history_change(&h, DAY0 + 8, 0, 7, 140, &c));
	zassert_equal(c, 40);
	/* Wanted day 3: nearest earlier record is day 1, two days before: still fine */
	zassert_true(history_change(&h, DAY0 + 10, 0, 7, 140, &c));
	zassert_equal(c, 40);
	/* Wanted day 5: four days after the record, beyond the tolerance */
	zassert_false(history_change(&h, DAY0 + 12, 0, 7, 140, &c));
}

ZTEST(history, test_the_record_keeps_only_the_latest_days)
{
	before(NULL);
	int64_t c;

	for (int d = 0; d < HISTORY_DAYS + 10; d++) {
		zassert_true(history_record(&h, DAY0 + d, 0, d));
	}
	zassert_equal(h.count, HISTORY_DAYS);
	zassert_equal(h.entry[0].day, DAY0 + 10, "the oldest ten days were dropped");
	zassert_equal(h.entry[HISTORY_DAYS - 1].day, DAY0 + HISTORY_DAYS + 9);

	zassert_true(history_change(&h, DAY0 + HISTORY_DAYS + 9, 0, 30, 49, &c));
	zassert_equal(c, 30);
}

ZTEST(history, test_an_earlier_day_is_ignored)
{
	before(NULL);
	zassert_true(history_record(&h, DAY0 + 5, 0, 10));
	zassert_false(history_record(&h, DAY0 + 4, 0, 99), "the clock went backwards");
	zassert_equal(h.count, 1);
	zassert_equal(h.entry[0].value[0], 10);
}

ZTEST(history, test_bad_arguments)
{
	before(NULL);
	int64_t c;

	zassert_false(history_record(&h, DAY0, -1, 1));
	zassert_false(history_record(&h, DAY0, HISTORY_METRICS, 1));
	zassert_false(history_change(&h, DAY0, -1, 7, 1, &c));
	zassert_false(history_change(&h, 3, 0, 7, 1, &c), "a week before day 3 does not exist");
}

ZTEST_SUITE(history, NULL, NULL, before, NULL, NULL);
