/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Clock and weather: the time, the date and the current weather, one after the other. The time
 * page follows the minute. The middle button pauses the rotation.
 */

#include <string.h>

#include <zephyr/kernel.h>

#include "app.h"
#include "clock.h"
#include "config.h"
#include "format.h"
#include "logos.h"
#include "weather.h"
#include "wmo.h"

#define TIME_HOLD_MS    7000
#define DATE_HOLD_MS    3000
#define WEATHER_HOLD_MS 4000

#define COLOR_TIME   0xffffff
#define COLOR_DATE   0x80c8ff
#define COLOR_UNSET  0x505050
#define COLOR_PAUSE  0x404040

enum page {
	PAGE_TIME,
	PAGE_DATE,
	PAGE_WEATHER,
	PAGE_COUNT,
};

static enum page page;
static int64_t page_start;
static bool first;
static bool redraw;
static bool paused;
static char drawn_time[12]; /* what the time page last showed, to spot the minute changing */

static bool page_available(enum page p)
{
	struct weather w;

	switch (p) {
	case PAGE_DATE:
		return clock_is_set();
	case PAGE_WEATHER:
		return weather_get(&w);
	default:
		return true;
	}
}

static enum page next_page(enum page from)
{
	for (int i = 1; i <= PAGE_COUNT; i++) {
		enum page p = (from + i) % PAGE_COUNT;

		if (page_available(p)) {
			return p;
		}
	}
	return from;
}

static void time_text(char *buf, size_t len)
{
	struct civil_time t;

	if (clock_local(&t)) {
		format_time(buf, len, t.hour, t.minute, config_get()->time_format == TIME_12H);
	} else {
		strncpy(buf, "--:--", len);
	}
}

static void date_text(char *buf, size_t len)
{
	struct civil_time t;

	if (clock_local(&t)) {
		format_date(buf, len, t.month, t.day, config_get()->date_format);
	} else {
		buf[0] = '\0';
	}
}

static uint32_t temperature_color(int tenths)
{
	if (tenths <= 0) {
		return 0x78beff; /* freezing */
	} else if (tenths <= 100) {
		return 0xb4dcff; /* cold */
	} else if (tenths <= 200) {
		return 0xffffff; /* mild */
	} else if (tenths <= 280) {
		return 0xffd23c; /* warm */
	}
	return 0xff6428; /* hot */
}

static const uint8_t *weather_icon(const struct weather *w)
{
	switch (wmo_kind(w->code, w->is_day)) {
	case WX_SUN:
		return logo_wx_sun;
	case WX_MOON:
		return logo_wx_moon;
	case WX_PARTLY:
		return logo_wx_partly;
	case WX_FOG:
		return logo_wx_fog;
	case WX_RAIN:
		return logo_wx_rain;
	case WX_SNOW:
		return logo_wx_snow;
	case WX_STORM:
		return logo_wx_storm;
	case WX_CLOUD:
	default:
		return logo_wx_cloud;
	}
}

static void draw_centered(struct gfx_fb *fb, const char *text, uint32_t color)
{
	gfx_text(fb, (GFX_W - gfx_text_width(text)) / 2, 1, text, color);
}

static void draw_page(struct gfx_fb *fb)
{
	char text[16];
	struct weather w;

	switch (page) {
	case PAGE_TIME:
		time_text(text, sizeof(text));
		strncpy(drawn_time, text, sizeof(drawn_time));
		draw_centered(fb, text, clock_is_set() ? COLOR_TIME : COLOR_UNSET);
		break;

	case PAGE_DATE:
		date_text(text, sizeof(text));
		draw_centered(fb, text, COLOR_DATE);
		break;

	case PAGE_WEATHER:
		if (weather_get(&w)) {
			format_temperature(text, sizeof(text), w.temperature);
			gfx_sprite(fb, 0, 0, weather_icon(&w));
			/* Centre the temperature in the 23 pixels right of the icon */
			gfx_text(fb, 9 + (GFX_W - 9 - gfx_text_width(text)) / 2, 1, text,
				 temperature_color(w.temperature));
		}
		break;

	default:
		break;
	}

	if (paused) {
		gfx_pixel(fb, GFX_W - 1, 0, COLOR_PAUSE);
	}
}

static bool page_finished(int64_t now)
{
	int64_t elapsed = now - page_start;

	switch (page) {
	case PAGE_TIME:
		return elapsed >= TIME_HOLD_MS;
	case PAGE_DATE:
		return elapsed >= DATE_HOLD_MS;
	default:
		return elapsed >= WEATHER_HOLD_MS;
	}
}

static void enter(int64_t now)
{
	first = true;
	page = PAGE_TIME;
	page_start = now;
}

static enum app_result update(struct gfx_fb *fb, int64_t now)
{
	char text[16];

	if (first) {
		first = redraw = false;
		page_start = now; /* counted from here, not from enter(): a title may have come first */
		draw_page(fb);
		return APP_REDRAW;
	}

	if (!paused && page_finished(now)) {
		enum page next = next_page(page);

		page_start = now;
		if (next == page) {
			return APP_IDLE; /* the only page there is: nothing to slide to */
		}
		page = next;
		draw_page(fb);
		return APP_SLIDE_UP;
	}

	if (redraw) {
		redraw = false;
		draw_page(fb);
		return APP_REDRAW;
	}

	if (page == PAGE_TIME) {
		/* The time page follows the clock: redraw when the minute (or the format) changes */
		time_text(text, sizeof(text));
		if (strcmp(text, drawn_time) != 0) {
			draw_page(fb);
			return APP_REDRAW;
		}
	}

	return APP_IDLE;
}

static void middle(int64_t now)
{
	paused = !paused;
	/* Resuming gives the page on display its full time again */
	page_start = now;
	redraw = true;
}

const struct app app_clock = {
	.title = "TIME",
	.enter = enter,
	.update = update,
	.middle = middle,
};
