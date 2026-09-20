/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_SCROLL_H_
#define TC001_SCROLL_H_

#include <stdint.h>

/*
 * Where text of @p text_width pixels starts in a view of @p view_width pixels, @p elapsed_ms after
 * it was first shown. Text that fits is centred. Text that does not fit is shown from its start
 * for a moment, scrolls to its end, waits, and starts over.
 */
int scroll_x(int text_width, int view_width, int64_t elapsed_ms);

/** Whether the position for this text is still changing, so a redraw is needed each pixel. */
int scroll_cycle_ms(int text_width, int view_width);

#endif /* TC001_SCROLL_H_ */
