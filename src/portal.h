/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_PORTAL_H_
#define TC001_PORTAL_H_

/**
 * Start the web server on port 80 (only the first call does anything).
 *
 * What a request gets depends on the address it arrived on. On the clock's own access point it is
 * the Wi-Fi setup page: the saved networks with a button to forget each, and a form to add one; the
 * board reboots shortly after a network is saved so it can connect to it. On the home network it
 * is the settings page: which apps are shown, and the date and time format.
 */
int portal_start(void);

#endif /* TC001_PORTAL_H_ */
