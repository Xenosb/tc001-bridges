/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_LIGHT_H_
#define TC001_LIGHT_H_

#include <stdint.h>

/** Set up the ambient light sensor input. Returns 0 on success. */
int light_init(void);

/**
 * Read the ambient light sensor, averaged over several samples.
 *
 * @return the ADC count (12 bit), or a negative errno on failure
 */
int light_read(void);

#endif /* TC001_LIGHT_H_ */
