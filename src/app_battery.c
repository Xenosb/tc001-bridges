/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Battery level: a battery symbol filled to the charge, and the percentage beside it. While the
 * battery seems to be charging, the rightmost column is a yellow bar with a dark block travelling
 * up it, round and round.
 *
 * The clock has no signal that says it is charging, so this is inferred from the voltage: a battery
 * on the charger sits at or near full voltage, or climbs. It can be wrong for a while, for example
 * just after unplugging a full battery.
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>

#include "app.h"
#include "battery.h"

#define SAMPLE_MS        10000
#define TREND_SAMPLES    60 /* ten minutes of samples */
#define RISING_MV        20 /* climbing this much over the trend window means charging */
#define NEAR_FULL_MV     4130

#define COLOR_OUTLINE 0xb0b0b0
#define COLOR_GOOD    0x64dc8c
#define COLOR_LOW     0xffc83c
#define COLOR_EMPTY   0xff5050
#define COLOR_TEXT    0xffffff
#define COLOR_BAR     0xffd23c

/* The battery fills the height of the display: a 14 pixel body and a 2 pixel terminal */
#define BATTERY_W        16
#define BATTERY_H        8
#define INTERIOR_W       12 /* columns 1 to 12, rows 1 to 6 */
#define TEXT_LEFT        BATTERY_W
#define TEXT_AREA        (GFX_W - 1 - TEXT_LEFT) /* up to the column before the charging bar */

/* The charging bar: the last column, with a dark block moving up it */
#define BAR_X        (GFX_W - 1)
#define BAR_BLOCK    3
#define BAR_STEP_MS  120

static const char *const battery_shape[BATTERY_H] = {
	"XXXXXXXXXXXXXX..",
	"X............X..",
	"X............XXX",
	"X............XXX",
	"X............XXX",
	"X............XXX",
	"X............X..",
	"XXXXXXXXXXXXXX..",
};

static int history[TREND_SAMPLES];
static size_t history_count;
static size_t history_next;
static int64_t next_sample;
static bool have_reading;
static int millivolts;
static int percent;
static bool charging;
static bool redraw;
static bool ready = true;
static int drawn_bar_step = -1;

static void take_sample(void)
{
	int mv = battery_millivolts(NULL);
	int oldest;

	if (mv < 0) {
		ready = false;
		return;
	}
	ready = true;

	if (history_count == TREND_SAMPLES) {
		oldest = history[history_next];
	} else {
		oldest = history_count > 0 ? history[0] : mv;
	}
	history[history_next] = mv;
	history_next = (history_next + 1) % TREND_SAMPLES;
	if (history_count < TREND_SAMPLES) {
		history_count++;
	}

	millivolts = mv;
	percent = battery_percent(mv);
	charging = mv >= NEAR_FULL_MV || mv - oldest >= RISING_MV;
	have_reading = true;
	redraw = true;
}

/* Which position of the charging animation it is at @p now */
static int bar_step(int64_t now)
{
	return (now / BAR_STEP_MS) % GFX_H;
}

static void draw(struct gfx_fb *fb, int64_t now)
{
	char text[8];
	int filled = (percent * INTERIOR_W + 50) / 100;
	uint32_t color = percent > 50 ? COLOR_GOOD : (percent > 20 ? COLOR_LOW : COLOR_EMPTY);
	int step = bar_step(now);

	if (percent > 0 && filled == 0) {
		filled = 1; /* almost empty is not the same as empty */
	}

	gfx_mask(fb, 0, 0, battery_shape, BATTERY_H, COLOR_OUTLINE);
	for (int col = 0; col < filled; col++) {
		for (int row = 1; row < BATTERY_H - 1; row++) {
			gfx_pixel(fb, 1 + col, row, color);
		}
	}

	snprintf(text, sizeof(text), "%d", percent);
	gfx_text(fb, TEXT_LEFT + (TEXT_AREA - gfx_text_width(text)) / 2, 1, text, COLOR_TEXT);

	if (charging) {
		/* The dark block's top row moves up by one each step and wraps round the column */
		int top = (GFX_H - 1) - step;

		for (int row = 0; row < GFX_H; row++) {
			if ((row - top + GFX_H) % GFX_H >= BAR_BLOCK) {
				gfx_pixel(fb, BAR_X, row, COLOR_BAR);
			}
		}
	}
	drawn_bar_step = step;
}

static void enter(int64_t now)
{
	next_sample = now;
	redraw = true;
}

static enum app_result update(struct gfx_fb *fb, int64_t now)
{
	if (now >= next_sample) {
		take_sample();
		next_sample = now + SAMPLE_MS;
	}

	/* While charging, the bar moves on every step of its animation */
	if (charging && bar_step(now) != drawn_bar_step) {
		redraw = true;
	}

	if (!redraw) {
		return APP_IDLE;
	}
	redraw = false;

	if (!have_reading) {
		gfx_text(fb, (GFX_W - gfx_text_width("N/A")) / 2, 1, "N/A", 0x505050);
	} else {
		draw(fb, now);
	}
	return APP_REDRAW;
}

const struct app app_battery = {
	.title = "BATTERY",
	.enter = enter,
	.update = update,
};
