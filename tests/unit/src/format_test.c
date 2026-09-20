/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>

#include <zephyr/ztest.h>

#include "font3x5.h"
#include "format.h"

static void check(uint32_t value, const char *expected)
{
	char buf[12];

	format_count(buf, sizeof(buf), value);
	zassert_ok(strcmp(buf, expected), "%u -> \"%s\", expected \"%s\"", (unsigned int)value, buf,
		   expected);
	/* Must fit between a logo and an icon: four glyphs of 4 pixels, minus the last gap */
	zassert_true(strlen(buf) <= 4);
}

ZTEST(format, test_counts)
{
	check(0, "0");
	check(999, "999");
	check(2252, "2252");
	check(9999, "9999");
	check(10000, "10K");
	check(99999, "99K");
	check(100000, "100K");
	check(999999, "999K");
	check(1000000, "1.0M");
	check(1234567, "1.2M");
	check(12345678, "12M");
}

/* Rows of a glyph as text, e.g. "###/#../#../#../###" for 'C' */
static void glyph_rows(char c, char out[20])
{
	uint16_t g = font3x5[c - FONT3X5_FIRST];
	size_t n = 0;

	for (int row = 0; row < FONT3X5_H; row++) {
		for (int col = 0; col < FONT3X5_W; col++) {
			out[n++] = (g >> (row * FONT3X5_W + col)) & 1 ? '#' : '.';
		}
		out[n++] = '/';
	}
	out[n - 1] = '\0';
}

ZTEST(format, test_font_matches_the_ttf)
{
	char rows[20];

	/* Spot checks against what tools/gen_font.py printed for these glyphs */
	glyph_rows('C', rows);
	zassert_ok(strcmp(rows, "###/#../#../#../###"), "%s", rows);
	glyph_rows('#', rows);
	zassert_ok(strcmp(rows, "#.#/###/#.#/###/#.#"), "%s", rows);
	glyph_rows('0', rows);
	zassert_ok(strcmp(rows, "###/#.#/#.#/#.#/###"), "%s", rows);
	glyph_rows('.', rows);
	zassert_ok(strcmp(rows, ".../.../.../.../.#."), "%s", rows);
	glyph_rows(' ', rows);
	zassert_ok(strcmp(rows, ".../.../.../.../..."), "%s", rows);
}

ZTEST_SUITE(format, NULL, NULL, NULL, NULL, NULL);
