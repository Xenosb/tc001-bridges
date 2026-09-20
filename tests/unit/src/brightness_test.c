/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <zephyr/ztest.h>

#include "brightness.h"
#include "scroll.h"

ZTEST(brightness, test_darker_room_is_dimmer)
{
	int last = BRIGHTNESS_MAX + 1;

	/* Counts run from bright (low) to dark (high): the display dims as the count rises */
	for (int raw = 1; raw <= 4095; raw += 7) {
		int b = brightness_from_light(raw);

		zassert_true(b >= BRIGHTNESS_MIN && b <= BRIGHTNESS_MAX, "raw %d -> %d", raw, b);
		zassert_true(b <= last, "raw %d got brighter: %d after %d", raw, b, last);
		last = b;
	}
}

ZTEST(brightness, test_ends_of_the_range)
{
	zassert_equal(brightness_from_light(CONFIG_TC001_LDR_BRIGHT_COUNT), BRIGHTNESS_MAX);
	zassert_equal(brightness_from_light(1), BRIGHTNESS_MAX);
	zassert_equal(brightness_from_light(CONFIG_TC001_LDR_DARK_COUNT), BRIGHTNESS_MIN);
	zassert_equal(brightness_from_light(4095), BRIGHTNESS_MIN);
}

ZTEST(brightness, test_a_normal_room_is_somewhere_in_between)
{
	int b = brightness_from_light(400);

	zassert_true(b > 40 && b < 90, "%d", b);
}

ZTEST(brightness, test_manual_level_ignores_the_sensor)
{
	struct config cfg = {.brightness = 35, .auto_brightness = 0, .brightness_bias = 20};

	zassert_equal(brightness_target(&cfg, 3000), 35);
	zassert_equal(brightness_target(&cfg, 100), 35);
}

ZTEST(brightness, test_bias_moves_the_automatic_level_and_is_clamped)
{
	struct config cfg = {.auto_brightness = 1, .brightness_bias = 0};
	int base = brightness_target(&cfg, 400);

	cfg.brightness_bias = 10;
	zassert_equal(brightness_target(&cfg, 400), base + 10);
	cfg.brightness_bias = -10;
	zassert_equal(brightness_target(&cfg, 400), base - 10);

	cfg.brightness_bias = 50;
	zassert_equal(brightness_target(&cfg, 100), BRIGHTNESS_MAX, "never above the maximum");
	cfg.brightness_bias = -50;
	zassert_equal(brightness_target(&cfg, 4000), BRIGHTNESS_MIN, "never below the minimum");
}

ZTEST(brightness, test_scrolling_text)
{
	/* Text that fits is centred and never moves */
	zassert_equal(scroll_x(20, 32, 0), 6);
	zassert_equal(scroll_x(20, 32, 123456), 6);
	zassert_equal(scroll_cycle_ms(20, 32), 0);

	/* Text of 44 pixels in a 32 pixel view overflows by 12 */
	zassert_equal(scroll_x(44, 32, 0), 0, "starts at its beginning");
	zassert_equal(scroll_x(44, 32, 999), 0, "and waits");
	zassert_true(scroll_x(44, 32, 1500) < 0, "then moves");
	zassert_equal(scroll_x(44, 32, 1000 + 12 * 60 + 100), -12, "ends at its end");
	zassert_equal(scroll_x(44, 32, scroll_cycle_ms(44, 32)), 0, "and starts over");
	zassert_equal(scroll_x(44, 32, scroll_cycle_ms(44, 32) * 3 + 10), 0);

	for (int64_t t = 0; t < 20000; t += 37) {
		int x = scroll_x(44, 32, t);

		zassert_true(x <= 0 && x >= -12, "t %lld -> %d", (long long)t, x);
	}
}

ZTEST_SUITE(brightness, NULL, NULL, NULL, NULL, NULL);
