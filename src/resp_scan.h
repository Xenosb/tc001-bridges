/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_RESP_SCAN_H_
#define TC001_RESP_SCAN_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Streaming extraction of numeric JSON fields from an HTTP response.
 *
 * The board only needs a few numbers out of responses that can be several kilobytes, so instead
 * of buffering and parsing the whole document this scans the byte stream as it arrives. It skips
 * the headers, checks the status line, then looks for the first `"key": <number>` in the body
 * for each requested field. Data may be fed in arbitrarily sized pieces.
 *
 * Numbers may be negative and have decimals. They are returned as integers scaled by ten to the
 * power of the field's `decimals`, so "14.36" with two decimals is 1436. Digits beyond that are
 * dropped, not rounded.
 */

#define RESP_SCAN_MAX_KEY 32

struct resp_field {
	/* Set by the caller */
	const char *key;   /* field name without quotes */
	uint8_t decimals;  /* digits after the point to keep */

	/* Filled in by the scanner */
	int64_t value;
	bool found;

	/* Private */
	size_t pos_;
};

struct resp_scan {
	struct resp_field *fields;
	size_t count;

	/* Status line, e.g. "HTTP/1.0 200 OK", is collected up to its first space */
	char status_line[16];
	size_t status_len;
	int status;

	/* Headers end at the first blank line */
	unsigned int header_match;
	bool in_body;

	enum {
		SCAN_KEY,   /* looking for any of the keys */
		SCAN_COLON, /* found a key: expect optional spaces, a colon, optional spaces */
		SCAN_VALUE, /* reading the number */
	} state;
	struct resp_field *active;
	bool have_colon;
	bool have_digits;
	bool negative;
	bool in_fraction;
	uint8_t fraction_digits;
	int64_t value;
};

/** @p fields is an array of @p count entries with `key` and `decimals` set. */
void resp_scan_init(struct resp_scan *s, struct resp_field *fields, size_t count);

/**
 * Feed the next piece of the response.
 *
 * @return true once every field has been found, so no more data is needed
 */
bool resp_scan_feed(struct resp_scan *s, const char *data, size_t len);

/**
 * Call when the connection has closed. A number that ends exactly at the end of the data is
 * only known to be complete now.
 *
 * @return true if every field was found
 */
bool resp_scan_finish(struct resp_scan *s);

/** HTTP status code, or 0 if the status line has not been seen (or is malformed). */
int resp_scan_status(const struct resp_scan *s);

#endif /* TC001_RESP_SCAN_H_ */
