/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_GFX_H_
#define TC001_GFX_H_

#include <stdint.h>

/*
 * Minimal drawing for the 32x8 LED matrix: a small RGB framebuffer with text,
 * sprites and a vertical slide between two frames. Colours are 0xRRGGBB.
 */

#define GFX_W 32
#define GFX_H 8

struct gfx_fb {
	uint8_t px[GFX_H][GFX_W][3];
};

/** Bring up the display. Returns 0 on success, negative errno on failure. */
int gfx_init(void);

void gfx_clear(struct gfx_fb *fb);

/** Set one pixel; coordinates outside the frame are ignored. */
void gfx_pixel(struct gfx_fb *fb, int x, int y, uint32_t color);

/** Width in pixels of @p text drawn with gfx_text() */
int gfx_text_width(const char *text);

/** Draw text with the 3x5 font. The top left of the first glyph is at (x, y); clipped to the frame. */
void gfx_text(struct gfx_fb *fb, int x, int y, const char *text, uint32_t color);

/** Draw an 8x8 RGB888 sprite (row by row, off pixels are black and left transparent). */
void gfx_sprite(struct gfx_fb *fb, int x, int y, const uint8_t *rgb888);

/** Draw a shape given as text rows, where 'X' is a lit pixel and anything else is left alone. */
void gfx_mask(struct gfx_fb *fb, int x, int y, const char *const *rows, int height, uint32_t color);

/**
 * Build the frame @p offset rows into a slide up from @p from to @p to.
 * At offset 0 the result is @p from, at GFX_H it is @p to.
 */
void gfx_slide(struct gfx_fb *out, const struct gfx_fb *from, const struct gfx_fb *to, int offset);

/**
 * Build the frame @p offset columns into a slide sideways from @p from to @p to.
 * @p dir is +1 when the new frame arrives from the right, -1 when it arrives from the left.
 */
void gfx_slide_h(struct gfx_fb *out, const struct gfx_fb *from, const struct gfx_fb *to, int offset,
		 int dir);

/** Set how bright everything drawn is, in percent (1 to 100). */
void gfx_set_brightness(unsigned int percent);

/** Send a frame to the LEDs, scaled by the brightness. */
void gfx_present(const struct gfx_fb *fb);

#endif /* TC001_GFX_H_ */
