/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * "Bridges total": a title, then the stars and downloads of both bridges, each as a logo with
 * its number beside it. The pages rotate on their own; the middle button pauses that.
 */

#include <string.h>

#include <zephyr/kernel.h>

#include "app.h"
#include "format.h"
#include "icons.h"
#include "logos.h"

#define STAT_HOLD_MS  3500

#define COLOR_NOTE  0x787878
#define COLOR_PAUSE 0x404040
#define COLOR_STAR  0xffd23c
#define COLOR_ARROW 0x64dc8c

/* Pages, in the order they rotate after the title */
static const struct {
	enum stat_bridge bridge;
	enum stat_metric metric;
	const uint8_t *logo;
	uint32_t color;
} page_order[] = {
	{STAT_BRIDGE_CSHARP, STAT_METRIC_STARS, logo_csharp, 0xb978ff},
	{STAT_BRIDGE_RUST, STAT_METRIC_STARS, logo_rust, 0xff6e1e},
	{STAT_BRIDGE_CSHARP, STAT_METRIC_DOWNLOADS, logo_nuget, 0xb978ff},
	{STAT_BRIDGE_RUST, STAT_METRIC_DOWNLOADS, logo_crate, 0xff6e1e},
};

static bool is_stars(enum stat_metric metric)
{
	return metric == STAT_METRIC_STARS;
}

struct page {
	const uint8_t *logo;
	const char *const *icon; /* star for stars, down arrow for downloads */
	uint32_t icon_color;
	uint32_t color;
	uint32_t value;
};

#define MAX_PAGES ARRAY_SIZE(page_order)

static struct page pages[MAX_PAGES];
static size_t page_count;
static size_t page_index;
static bool paused;
static bool redraw;
static bool first; /* the next frame is the first after enter(): show it without a slide */
static int64_t next_switch;

static void draw_page(struct gfx_fb *fb, const struct page *p)
{
	char number[12];
	int group_width, x;

	format_count(number, sizeof(number), p->value);
	gfx_sprite(fb, 0, 0, p->logo);

	/* Centre the number and its icon, one pixel apart, in the 23 pixels right of the logo */
	group_width = gfx_text_width(number) + 1 + ICON_W;
	x = 9 + (GFX_W - 9 - group_width) / 2;
	gfx_text(fb, x, 1, number, p->color);
	gfx_mask(fb, x + gfx_text_width(number) + 1, 1, p->icon, ICON_H, p->icon_color);

	if (paused) {
		gfx_pixel(fb, GFX_W - 1, 0, COLOR_PAUSE);
	}
}

void app_bridges_set_items(const struct stat_item *items, size_t count)
{
	struct page next[MAX_PAGES];
	size_t n = 0;

	for (size_t p = 0; p < ARRAY_SIZE(page_order); p++) {
		for (size_t i = 0; i < count; i++) {
			if (items[i].bridge == page_order[p].bridge &&
			    items[i].metric == page_order[p].metric) {
				next[n++] = (struct page){
					.logo = page_order[p].logo,
					.icon = is_stars(page_order[p].metric) ? icon_star
									       : icon_arrow_down,
					.icon_color = is_stars(page_order[p].metric) ? COLOR_STAR
										     : COLOR_ARROW,
					.color = page_order[p].color,
					.value = items[i].value,
				};
				break;
			}
		}
	}
	if (n == 0) {
		/* Nothing usable in this update: keep whatever is already showing */
		return;
	}

	memcpy(pages, next, n * sizeof(next[0]));
	if (page_count == 0) {
		page_index = 0;
		first = true;
	}
	page_count = n;
	page_index %= page_count;
	redraw = true;
}

static void enter(int64_t now)
{
	first = true;
	next_switch = now + (page_count ? STAT_HOLD_MS : 0);
}

static enum app_result update(struct gfx_fb *fb, int64_t now)
{
	if (page_count == 0) {
		/* No data yet */
		if (first || redraw) {
			gfx_text(fb, (GFX_W - gfx_text_width("SYNC")) / 2, 1, "SYNC", COLOR_NOTE);
			first = redraw = false;
			return APP_REDRAW;
		}
		return APP_IDLE;
	}

	if (first) {
		draw_page(fb, &pages[page_index]);
		first = redraw = false;
		next_switch = now + STAT_HOLD_MS;
		return APP_REDRAW;
	}

	if (!paused && now >= next_switch) {
		page_index = (page_index + 1) % page_count;
		draw_page(fb, &pages[page_index]);
		next_switch = now + STAT_HOLD_MS;
		redraw = false;
		return APP_SLIDE_UP;
	}

	if (redraw) {
		/* New numbers, or the pause marker changed: same page, no slide */
		draw_page(fb, &pages[page_index]);
		redraw = false;
		return APP_REDRAW;
	}

	return APP_IDLE;
}

static void middle(int64_t now)
{
	paused = !paused;
	/* Resuming gives the page on display its full time again */
	next_switch = now + (page_count ? STAT_HOLD_MS : 0);
	redraw = true;
}

const struct app app_bridges_total = {
	.title = "TOTAL",
	.enter = enter,
	.update = update,
	.middle = middle,
};
