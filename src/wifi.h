/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_WIFI_H_
#define TC001_WIFI_H_

#include <stdbool.h>
#include <stddef.h>

#include <zephyr/net/net_ip.h>

/**
 * Scan for the saved networks and join the strongest one that is in range.
 *
 * Makes up to two connection attempts. Returns -ENOENT when none of the saved
 * networks is available (or none is saved), -ECONNREFUSED when both attempts
 * failed.
 *
 * @return 0 once connected and an IPv4 address has been assigned
 */
int wifi_connect_saved(void);

/** Name of the network the clock is connected to, or "" when it is not connected. */
const char *wifi_ssid(void);

/** The clock's address on that network as text, or "" when it is not connected. */
const char *wifi_address(void);

/**
 * Start the setup access point with its own DHCP server.
 *
 * @param ip     Receives the AP's IPv4 address as text
 * @param ip_len Size of @p ip
 * @return 0 on success, negative errno on failure
 */
int wifi_start_ap(char *ip, size_t ip_len);

/** Whether @p addr is the access point's own address, i.e. a request on it came in over the AP. */
bool wifi_is_ap_address(const struct net_in_addr *addr);

/** Stop the setup access point, leaving any connection to a network alone. */
void wifi_stop_ap(void);

#endif /* TC001_WIFI_H_ */
