/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdbool.h>
#include <string.h>

#include <zephyr/ztest.h>

#include "color.h"
#include "font3x5.h"
#include "qt_logo.h"

static unsigned int red(uint32_t c) { return c >> 16 & 0xff; }
static unsigned int green(uint32_t c) { return c >> 8 & 0xff; }
static unsigned int blue(uint32_t c) { return c & 0xff; }

ZTEST(graphics, test_color_wheel_landmarks)
{
	zassert_equal(color_hue(0), 0xff0000, "red");

	/* A third and two thirds of the way round: green, then blue (within rounding) */
	zassert_equal(green(color_hue(85)), 255);
	zassert_true(red(color_hue(85)) <= 2 && blue(color_hue(85)) == 0);
	zassert_equal(blue(color_hue(170)), 255);
	zassert_true(red(color_hue(170)) == 0 && green(color_hue(170)) <= 3);
}

ZTEST(graphics, test_color_wheel_wraps_smoothly)
{
	/* The last hue must sit next to the first, or the moving logo would jump at the wrap */
	uint32_t last = color_hue(255), first = color_hue(0);

	zassert_equal(red(last), 255);
	zassert_true(blue(last) <= 6, "0x%06x", last);
	zassert_equal(red(first), 255);
	zassert_equal(green(first) + blue(first), 0);
}

ZTEST(graphics, test_color_wheel_is_always_fully_saturated)
{
	for (int h = 0; h < 256; h++) {
		uint32_t c = color_hue(h);
		unsigned int r = c >> 16 & 0xff, g = c >> 8 & 0xff, b = c & 0xff;
		unsigned int max = MAX(r, MAX(g, b)), min = MIN(r, MIN(g, b));

		/* One channel at full, one at zero: every hue is as vivid as an LED can be */
		zassert_equal(max, 255, "hue %d -> 0x%06x", h, c);
		zassert_equal(min, 0, "hue %d -> 0x%06x", h, c);
	}
}

ZTEST(graphics, test_qt_logo_shape)
{
	for (int row = 0; row < QT_LOGO_H; row++) {
		zassert_equal(strlen(qt_logo[row]), QT_LOGO_W, "row %d", row);
	}

	/* Chamfered top left and bottom right, solid elsewhere along the edges */
	zassert_equal(qt_logo[0][0], '.');
	zassert_equal(qt_logo[0][1], '.');
	zassert_equal(qt_logo[1][0], '.');
	zassert_equal(qt_logo[QT_LOGO_H - 1][QT_LOGO_W - 1], '.');
	zassert_equal(qt_logo[QT_LOGO_H - 1][QT_LOGO_W - 2], '.');
	zassert_equal(qt_logo[QT_LOGO_H - 2][QT_LOGO_W - 1], '.');
	zassert_equal(qt_logo[0][QT_LOGO_W - 1], 'X');
	zassert_equal(qt_logo[QT_LOGO_H - 1][0], 'X');
}

/* The Q and t are cut out using the font's own shapes, so the logo and the text agree */
ZTEST(graphics, test_qt_logo_lettering_matches_the_font)
{
	uint16_t q = font3x5['Q' - FONT3X5_FIRST];
	uint16_t t = font3x5['t' - FONT3X5_FIRST];

	for (int row = 0; row < FONT3X5_H; row++) {
		for (int col = 0; col < FONT3X5_W; col++) {
			bool q_lit = q >> (row * FONT3X5_W + col) & 1;
			bool t_lit = t >> (row * FONT3X5_W + col) & 1;

			/* Q sits at columns 2-4 and t at 6-8, both from row 1; a lit font pixel is a hole */
			zassert_equal(qt_logo[1 + row][2 + col] != 'X', q_lit, "Q row %d col %d", row, col);
			zassert_equal(qt_logo[1 + row][6 + col] != 'X', t_lit, "t row %d col %d", row, col);
		}
	}
}

ZTEST_SUITE(graphics, NULL, NULL, NULL, NULL, NULL);
