/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Bridges trends: how much stars and downloads changed over the last week and the last month, a
 * page each: [logo] W +12 [icon], then [logo] M +40 [icon]. The pages rotate on their own; the
 * middle button pauses that. A change shows as "--" until the daily record reaches back far enough.
 */

#include <string.h>

#include <zephyr/kernel.h>

#include "app.h"
#include "format.h"
#include "history.h"
#include "icons.h"
#include "logos.h"

#define PAGE_HOLD_MS 3500

#define COLOR_UP     0x64dc8c
#define COLOR_FLAT   0x909090
#define COLOR_DOWN   0xff5050
#define COLOR_UNKNOWN 0x505050
#define COLOR_LABEL  0x606060
#define COLOR_STAR   0xffd23c
#define COLOR_ARROW  0x64dc8c
#define COLOR_PAUSE  0x404040

/* Pages, in the order they rotate; each item gets a week page and a month page */
static const struct {
	enum stat_bridge bridge;
	enum stat_metric metric;
	const uint8_t *logo;
} items_order[] = {
	{STAT_BRIDGE_CSHARP, STAT_METRIC_STARS, logo_csharp},
	{STAT_BRIDGE_RUST, STAT_METRIC_STARS, logo_rust},
	{STAT_BRIDGE_CSHARP, STAT_METRIC_DOWNLOADS, logo_nuget},
	{STAT_BRIDGE_RUST, STAT_METRIC_DOWNLOADS, logo_crate},
};

static const struct {
	int days;
	char label;
} periods[] = {
	{7, 'W'},
	{30, 'M'},
};

#define PAGE_COUNT (ARRAY_SIZE(items_order) * ARRAY_SIZE(periods))

static struct {
	bool have;
	uint32_t value;
} current[STAT_BRIDGE_COUNT][STAT_METRIC_COUNT];
static bool have_any;

static size_t page_index;
static bool paused;
static bool redraw;
static bool first;
static int64_t next_switch;

void app_trends_set_items(const struct stat_item *items, size_t count)
{
	for (size_t i = 0; i < count; i++) {
		current[items[i].bridge][items[i].metric].have = true;
		current[items[i].bridge][items[i].metric].value = items[i].value;
		have_any = true;
	}
	redraw = true;
}

static void draw_page(struct gfx_fb *fb)
{
	size_t item = page_index / ARRAY_SIZE(periods);
	size_t period = page_index % ARRAY_SIZE(periods);
	enum stat_bridge bridge = items_order[item].bridge;
	enum stat_metric metric = items_order[item].metric;
	bool is_stars = metric == STAT_METRIC_STARS;
	char text[12] = "--";
	uint32_t color = COLOR_UNKNOWN;
	int64_t change;
	int text_width, group_width, x;
	bool with_label;

	if (current[bridge][metric].have &&
	    history_change_over(bridge, metric, periods[period].days, current[bridge][metric].value,
				&change)) {
		format_delta(text, sizeof(text), change);
		color = change > 0 ? COLOR_UP : (change < 0 ? COLOR_DOWN : COLOR_FLAT);
	}

	gfx_sprite(fb, 0, 0, items_order[item].logo);

	/* Label (W or M), change and icon fit in the 23 pixels right of the logo, unless the
	 * change is wide: then the label is left out
	 */
	text_width = gfx_text_width(text);
	with_label = 4 + text_width + 1 + ICON_W <= GFX_W - 9;
	group_width = (with_label ? 4 : 0) + text_width + 1 + ICON_W;
	x = 9 + (GFX_W - 9 - group_width) / 2;

	if (with_label) {
		char label[2] = {periods[period].label, '\0'};

		gfx_text(fb, x, 1, label, COLOR_LABEL);
		x += 4;
	}
	gfx_text(fb, x, 1, text, color);
	gfx_mask(fb, x + text_width + 1, 1, is_stars ? icon_star : icon_arrow_down, ICON_H,
		 is_stars ? COLOR_STAR : COLOR_ARROW);

	if (paused) {
		gfx_pixel(fb, GFX_W - 1, 0, COLOR_PAUSE);
	}
}

static void enter(int64_t now)
{
	first = true;
	next_switch = now + PAGE_HOLD_MS;
}

static enum app_result update(struct gfx_fb *fb, int64_t now)
{
	if (!have_any) {
		if (first || redraw) {
			gfx_text(fb, (GFX_W - gfx_text_width("SYNC")) / 2, 1, "SYNC", COLOR_UNKNOWN);
			first = redraw = false;
			return APP_REDRAW;
		}
		return APP_IDLE;
	}

	if (first) {
		first = redraw = false;
		draw_page(fb);
		next_switch = now + PAGE_HOLD_MS;
		return APP_REDRAW;
	}

	if (!paused && now >= next_switch) {
		page_index = (page_index + 1) % PAGE_COUNT;
		next_switch = now + PAGE_HOLD_MS;
		redraw = false;
		draw_page(fb);
		return APP_SLIDE_UP;
	}

	if (redraw) {
		redraw = false;
		draw_page(fb);
		return APP_REDRAW;
	}

	return APP_IDLE;
}

static void middle(int64_t now)
{
	paused = !paused;
	next_switch = now + PAGE_HOLD_MS;
	redraw = true;
}

const struct app app_bridges_trends = {
	.title = "TREND",
	.enter = enter,
	.update = update,
	.middle = middle,
};
