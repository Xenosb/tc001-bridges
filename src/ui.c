/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * One thread owns the display. It shows a status word or a scrolling text while the clock is
 * starting up or being set up, and afterwards one of the apps (see app.h), which the left and
 * right buttons switch between with a sideways slide.
 *
 * The other threads only change the state below; the display thread looks at it every tick.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app.h"
#include "brightness.h"
#include "config.h"
#include "gfx.h"
#include "ui.h"

LOG_MODULE_REGISTER(ui, LOG_LEVEL_INF);

#define TICK_MS          40
#define SLIDE_STEP_MS    35
#define H_SLIDE_STEP_MS  16
#define H_SLIDE_STEP_PX  2
#define MARQUEE_STEP     1 /* pixels per tick */
#define BRIGHTNESS_TICK_MS 500
#define TITLE_MS 1500
#define TEXT_MAX         112

#define COLOR_STATUS  0x787878
#define COLOR_MARQUEE 0x00a0ff
#define COLOR_TITLE   0xc8c8c8

/* The apps that exist so far; the rest of enum app_id are not built yet and are skipped */
static const struct app *const registry[APP_COUNT] = {
	[APP_CLOCK] = &app_clock,
	[APP_BRIDGES_TOTAL] = &app_bridges_total,
	[APP_BRIDGES_TRENDS] = &app_bridges_trends,
	[APP_SCREENSAVER] = &app_screensaver,
	[APP_BATTERY] = &app_battery,
	[APP_SETTINGS] = &app_settings,
};

enum mode {
	MODE_STATUS,
	MODE_TEXT,
	MODE_APPS,
};

enum action {
	ACT_NONE,
	ACT_SLIDE_UP,
	ACT_SLIDE_SIDEWAYS,
};

K_THREAD_STACK_DEFINE(ui_stack, 2048);
static struct k_thread ui_thread;
static K_MUTEX_DEFINE(lock);

/* Shared state, guarded by `lock` */
static enum mode mode = MODE_STATUS;
static char text[TEXT_MAX + 1];
static bool redraw;
static int current = -1; /* enum app_id being shown */
static int app_step;     /* pending switch: -1, 0 or +1 */
static int resume_app = -1; /* the app a text was shown over, to return to */
static bool intro_active;   /* an app's title is on display, before its pages */
static int64_t intro_end;
static bool entry_pending;  /* an app was just chosen without a slide: show it at the next tick */
static bool entry_with_title;

/* Display thread only */
static struct gfx_fb shown;
static struct gfx_fb incoming;
static struct gfx_fb frame;
static int marquee_x;
static int slide_dir;
static int64_t next_brightness_tick;

/* The next enabled app in direction @p dir, or -1 if there is none. @p from may be -1. */
static int next_app(int from, int dir)
{
	const struct config *cfg = config_get();

	for (int i = 1; i <= APP_COUNT; i++) {
		int id = (((from + dir * i) % APP_COUNT) + APP_COUNT) % APP_COUNT;

		if (registry[id] != NULL && config_app_enabled(cfg, id)) {
			return id;
		}
	}
	return -1;
}

static void draw_status(struct gfx_fb *fb)
{
	gfx_clear(fb);
	gfx_text(fb, (GFX_W - gfx_text_width(text)) / 2, 1, text, COLOR_STATUS);
}

static void draw_marquee(struct gfx_fb *fb)
{
	gfx_clear(fb);
	gfx_text(fb, marquee_x, 1, text, COLOR_MARQUEE);
}

static void slide_up_to(const struct gfx_fb *to)
{
	for (int offset = 1; offset <= GFX_H; offset++) {
		gfx_slide(&frame, &shown, to, offset);
		gfx_present(&frame);
		k_msleep(SLIDE_STEP_MS);
	}
	shown = *to;
}

static void slide_sideways_to(const struct gfx_fb *to, int dir)
{
	for (int offset = H_SLIDE_STEP_PX; offset <= GFX_W; offset += H_SLIDE_STEP_PX) {
		gfx_slide_h(&frame, &shown, to, offset, dir);
		gfx_present(&frame);
		k_msleep(H_SLIDE_STEP_MS);
	}
	shown = *to;
}

static void show_now(const struct gfx_fb *fb)
{
	shown = *fb;
	gfx_present(&shown);
}

/*
 * Make the app the current one: its title first, if it has one, then (after TITLE_MS) its pages.
 * The frame to show goes into `incoming`.
 */
static void begin_app(int id, int64_t now, bool with_title)
{
	const struct app *app = registry[id];

	gfx_clear(&incoming);
	app->enter(now);

	if (with_title && app->title != NULL) {
		gfx_text(&incoming, (GFX_W - gfx_text_width(app->title)) / 2, 1, app->title, COLOR_TITLE);
		intro_active = true;
		intro_end = now + TITLE_MS;
	} else {
		intro_active = false;
		app->update(&incoming, now);
	}
}

/* One pass of the display thread, with the lock held. Says how `incoming` is to be shown. */
static enum action update_state(void)
{
	int64_t now = k_uptime_get();

	switch (mode) {
	case MODE_STATUS:
		if (redraw) {
			draw_status(&incoming);
			show_now(&incoming);
			redraw = false;
		}
		break;

	case MODE_TEXT:
		if (redraw) {
			marquee_x = GFX_W;
			redraw = false;
		}
		draw_marquee(&incoming);
		show_now(&incoming);
		marquee_x -= MARQUEE_STEP;
		if (marquee_x < -gfx_text_width(text)) {
			marquee_x = GFX_W;
		}
		break;

	case MODE_APPS:
		if (entry_pending) {
			/* Handed over from the status word or returning from a text: no slide */
			entry_pending = false;
			begin_app(current, now, entry_with_title);
			show_now(&incoming);
			break;
		}

		gfx_clear(&incoming);

		if (app_step != 0) {
			int dir = app_step;
			int next = next_app(current, dir);

			app_step = 0;
			if (next >= 0 && next != current) {
				current = next;
				begin_app(current, now, true);
				slide_dir = dir;
				return ACT_SLIDE_SIDEWAYS;
			}
			break;
		}

		if (intro_active) {
			if (now < intro_end) {
				break;
			}
			/* The title has had its time: slide up to the app's first page */
			intro_active = false;
			registry[current]->update(&incoming, now);
			return ACT_SLIDE_UP;
		}

		switch (registry[current]->update(&incoming, now)) {
		case APP_REDRAW:
			show_now(&incoming);
			break;
		case APP_SLIDE_UP:
			return ACT_SLIDE_UP;
		default:
			break;
		}
		break;
	}

	return ACT_NONE;
}

static void ui_main(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	while (1) {
		enum action action;

		k_mutex_lock(&lock, K_FOREVER);
		action = update_state();
		k_mutex_unlock(&lock);

		/* Follow the light sensor, or the level chosen in the menu; send the picture again if
		 * the brightness moved
		 */
		if (k_uptime_get() >= next_brightness_tick) {
			next_brightness_tick = k_uptime_get() + BRIGHTNESS_TICK_MS;
			if (brightness_tick() && action == ACT_NONE) {
				gfx_present(&shown);
			}
		}

		/* Sliding takes a few hundred ms; the state is only read again afterwards */
		if (action == ACT_SLIDE_UP) {
			slide_up_to(&incoming);
		} else if (action == ACT_SLIDE_SIDEWAYS) {
			slide_sideways_to(&incoming, slide_dir);
		}

		k_msleep(TICK_MS);
	}
}

int ui_init(void)
{
	int ret = gfx_init();

	if (ret) {
		return ret;
	}

	gfx_clear(&shown);
	gfx_present(&shown);

	k_thread_create(&ui_thread, ui_stack, K_THREAD_STACK_SIZEOF(ui_stack), ui_main, NULL, NULL,
			NULL, K_PRIO_PREEMPT(8), 0, K_NO_WAIT);
	k_thread_name_set(&ui_thread, "ui");

	return 0;
}

void ui_set_status(const char *status)
{
	k_mutex_lock(&lock, K_FOREVER);
	/* What an app shows is worth more than a status word */
	if (mode != MODE_APPS) {
		mode = MODE_STATUS;
		strncpy(text, status, TEXT_MAX);
		text[TEXT_MAX] = '\0';
		redraw = true;
	}
	k_mutex_unlock(&lock);
}

void ui_show_text(const char *message)
{
	k_mutex_lock(&lock, K_FOREVER);
	if (mode == MODE_APPS) {
		resume_app = current; /* to come back to */
	}
	mode = MODE_TEXT;
	strncpy(text, message, TEXT_MAX);
	text[TEXT_MAX] = '\0';
	redraw = true;
	k_mutex_unlock(&lock);
}

void ui_hide_text(void)
{
	k_mutex_lock(&lock, K_FOREVER);
	if (mode == MODE_TEXT && resume_app >= 0) {
		mode = MODE_APPS;
		current = resume_app;
		resume_app = -1;
		entry_pending = true;
		entry_with_title = false; /* it was on display just before: no need to introduce it */
	}
	k_mutex_unlock(&lock);
}

void ui_start_apps(void)
{
	k_mutex_lock(&lock, K_FOREVER);
	if (mode != MODE_APPS) {
		current = next_app(-1, 1);
		if (current >= 0) {
			mode = MODE_APPS;
			entry_pending = true;
			entry_with_title = true;
		} else {
			mode = MODE_STATUS;
			strcpy(text, "NONE");
			redraw = true;
		}
	}
	k_mutex_unlock(&lock);
}

void ui_set_items(const struct stat_item *items, size_t count)
{
	k_mutex_lock(&lock, K_FOREVER);
	app_bridges_set_items(items, count);
	app_trends_set_items(items, count);
	k_mutex_unlock(&lock);
}

void ui_step(int direction)
{
	k_mutex_lock(&lock, K_FOREVER);
	if (mode == MODE_APPS) {
		/* An app with a menu takes the buttons for itself while the menu is open */
		if (registry[current]->nav == NULL ||
		    !registry[current]->nav(direction, k_uptime_get())) {
			app_step = direction;
		}
	}
	k_mutex_unlock(&lock);
}

void ui_middle(void)
{
	k_mutex_lock(&lock, K_FOREVER);
	/* Not while the title is showing: the app has not started yet */
	if (mode == MODE_APPS && !intro_active && !entry_pending && registry[current]->middle != NULL) {
		registry[current]->middle(k_uptime_get());
	}
	k_mutex_unlock(&lock);
}
