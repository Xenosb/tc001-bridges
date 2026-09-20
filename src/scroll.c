/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "scroll.h"

#define LEAD_MS      1000 /* at the start before it scrolls */
#define TAIL_MS      1000 /* at the end after it scrolled */
#define MS_PER_PIXEL 60

int scroll_cycle_ms(int text_width, int view_width)
{
	int overflow = text_width - view_width;

	return overflow <= 0 ? 0 : LEAD_MS + overflow * MS_PER_PIXEL + TAIL_MS;
}

int scroll_x(int text_width, int view_width, int64_t elapsed_ms)
{
	int overflow = text_width - view_width;
	int64_t t;
	int moved;

	if (overflow <= 0) {
		return (view_width - text_width) / 2;
	}

	t = elapsed_ms % scroll_cycle_ms(text_width, view_width);
	if (t < LEAD_MS) {
		return 0;
	}
	moved = (t - LEAD_MS) / MS_PER_PIXEL;
	return -(moved < overflow ? moved : overflow);
}
