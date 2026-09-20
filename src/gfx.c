/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

#include "font3x5.h"
#include "gfx.h"

LOG_MODULE_REGISTER(gfx, LOG_LEVEL_INF);

static const struct device *const display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* What actually goes to the LEDs: the frame after brightness scaling */
static uint8_t wire[GFX_H * GFX_W * 3];
static unsigned int brightness = CONFIG_TC001_BRIGHTNESS;

int gfx_init(void)
{
	struct display_capabilities caps;

	if (!device_is_ready(display)) {
		LOG_ERR("Display %s is not ready", display->name);
		return -ENODEV;
	}

	display_get_capabilities(display, &caps);
	if (caps.x_resolution != GFX_W || caps.y_resolution != GFX_H ||
	    !(caps.supported_pixel_formats & PIXEL_FORMAT_RGB_888)) {
		LOG_ERR("Unexpected display: %ux%u, formats 0x%x", caps.x_resolution,
			caps.y_resolution, caps.supported_pixel_formats);
		return -ENOTSUP;
	}

	display_set_pixel_format(display, PIXEL_FORMAT_RGB_888);
	display_blanking_off(display);

	return 0;
}

void gfx_clear(struct gfx_fb *fb)
{
	memset(fb, 0, sizeof(*fb));
}

void gfx_pixel(struct gfx_fb *fb, int x, int y, uint32_t color)
{
	if (x < 0 || x >= GFX_W || y < 0 || y >= GFX_H) {
		return;
	}
	fb->px[y][x][0] = color >> 16;
	fb->px[y][x][1] = color >> 8;
	fb->px[y][x][2] = color;
}

int gfx_text_width(const char *text)
{
	int n = strlen(text);

	/* No spacing after the last glyph */
	return n ? n * FONT3X5_ADVANCE - (FONT3X5_ADVANCE - FONT3X5_W) : 0;
}

void gfx_text(struct gfx_fb *fb, int x, int y, const char *text, uint32_t color)
{
	for (; *text != '\0'; text++, x += FONT3X5_ADVANCE) {
		unsigned char c = *text;
		uint16_t glyph;

		if (c < FONT3X5_FIRST || c >= FONT3X5_FIRST + FONT3X5_GLYPHS) {
			c = '?';
		}
		glyph = font3x5[c - FONT3X5_FIRST];

		for (int row = 0; row < FONT3X5_H; row++) {
			for (int col = 0; col < FONT3X5_W; col++) {
				if (glyph & BIT(row * FONT3X5_W + col)) {
					gfx_pixel(fb, x + col, y + row, color);
				}
			}
		}
	}
}

void gfx_sprite(struct gfx_fb *fb, int x, int y, const uint8_t *rgb888)
{
	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {
			const uint8_t *p = &rgb888[(row * 8 + col) * 3];

			if (p[0] | p[1] | p[2]) {
				gfx_pixel(fb, x + col, y + row, p[0] << 16 | p[1] << 8 | p[2]);
			}
		}
	}
}

void gfx_mask(struct gfx_fb *fb, int x, int y, const char *const *rows, int height, uint32_t color)
{
	for (int row = 0; row < height; row++) {
		for (int col = 0; rows[row][col] != '\0'; col++) {
			if (rows[row][col] == 'X') {
				gfx_pixel(fb, x + col, y + row, color);
			}
		}
	}
}

void gfx_slide(struct gfx_fb *out, const struct gfx_fb *from, const struct gfx_fb *to, int offset)
{
	for (int y = 0; y < GFX_H; y++) {
		int src = y + offset;

		memcpy(out->px[y], src < GFX_H ? from->px[src] : to->px[src - GFX_H],
		       sizeof(out->px[y]));
	}
}

void gfx_slide_h(struct gfx_fb *out, const struct gfx_fb *from, const struct gfx_fb *to, int offset,
		 int dir)
{
	for (int y = 0; y < GFX_H; y++) {
		for (int x = 0; x < GFX_W; x++) {
			/* Column x of the output shows the old frame's column x + offset (moving
			 * left) or the new frame's column beyond its edge
			 */
			int src = dir > 0 ? x + offset : x - offset;
			const uint8_t *px;

			if (dir > 0) {
				px = src < GFX_W ? from->px[y][src] : to->px[y][src - GFX_W];
			} else {
				px = src >= 0 ? from->px[y][src] : to->px[y][src + GFX_W];
			}
			memcpy(out->px[y][x], px, 3);
		}
	}
}

void gfx_set_brightness(unsigned int percent)
{
	brightness = CLAMP(percent, 1, 100);
}

void gfx_present(const struct gfx_fb *fb)
{
	const uint8_t *in = &fb->px[0][0][0];
	struct display_buffer_descriptor desc = {
		.buf_size = sizeof(wire),
		.width = GFX_W,
		.height = GFX_H,
		.pitch = GFX_W,
	};

	for (size_t i = 0; i < sizeof(wire); i++) {
		wire[i] = in[i] * brightness / 100;
	}

	display_write(display, 0, 0, &desc, wire);
}
