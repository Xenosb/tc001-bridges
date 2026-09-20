/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_LOCATION_H_
#define TC001_LOCATION_H_

#include <stdbool.h>
#include <stdint.h>

/* Where the clock is, worked out from its public IP address */
struct location {
	bool valid;
	int32_t latitude;   /* ten-thousandths of a degree */
	int32_t longitude;  /* ten-thousandths of a degree */
	int32_t utc_offset; /* seconds, as last reported by the weather service */
};

/** Load the last known location from flash and apply its time zone. Call once, early. */
void location_init(void);

/** Look the location up again over the network. Returns 0 on success. */
int location_refresh(void);

/** Copy out the current location; returns false if there is none yet. */
bool location_get(struct location *out);

/** Record the time zone offset the weather service reported, and pass it to the clock. */
void location_set_utc_offset(int32_t seconds);

#endif /* TC001_LOCATION_H_ */
