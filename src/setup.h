/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_SETUP_H_
#define TC001_SETUP_H_

#include <stdbool.h>

/*
 * Setting up a Wi-Fi network: the clock opens its own access point and scrolls its name, password
 * and address across the display. A phone or computer joins it and opens the address to add a
 * network (see portal.c).
 */

/**
 * The clock has no network to join. Open the access point and stay in setup until a network has
 * been saved (which reboots), the middle button cancels, or nobody does anything for a while (then
 * reboot and try again). Returns when the middle button cancels, to show the apps without a
 * network instead of resetting the board; otherwise does not return.
 */
void setup_boot(void);

/**
 * Asked for from the menu, while the clock is connected to a network. The access point is opened
 * alongside that connection, so the clock keeps working; the display shows the details until the
 * middle button ends it, or it times out. Returns at once: the work happens in the background.
 */
void setup_request(void);

/** Whether the access point is up and its details are on display. */
bool setup_active(void);

/**
 * The middle button: end the setup. When it was asked for from the menu the access point stops and
 * the display goes back to where it was; when it was at boot, the access point stops and
 * setup_boot() returns so the apps can start instead.
 */
void setup_cancel(void);

#endif /* TC001_SETUP_H_ */
