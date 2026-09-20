/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>

#include "resp_scan.h"

static const char header_end[] = "\r\n\r\n";

/* Pattern is the quoted key, e.g. "downloads": character @p i of it */
static char pattern_char(const struct resp_field *f, size_t i, size_t key_len)
{
	if (i == 0 || i == key_len + 1) {
		return '"';
	}
	return f->key[i - 1];
}

static size_t key_length(const struct resp_field *f)
{
	return strnlen(f->key, RESP_SCAN_MAX_KEY);
}

void resp_scan_init(struct resp_scan *s, struct resp_field *fields, size_t count)
{
	memset(s, 0, sizeof(*s));
	s->fields = fields;
	s->count = count;
	s->state = SCAN_KEY;

	for (size_t i = 0; i < count; i++) {
		fields[i].value = 0;
		fields[i].found = false;
		fields[i].pos_ = 0;
	}
}

static bool all_found(const struct resp_scan *s)
{
	for (size_t i = 0; i < s->count; i++) {
		if (!s->fields[i].found) {
			return false;
		}
	}
	return true;
}

/* Restart key matching; @p c is the byte that just failed to continue a match */
static void restart_matching(struct resp_scan *s, char c)
{
	s->state = SCAN_KEY;
	s->active = NULL;
	for (size_t i = 0; i < s->count; i++) {
		/* The pattern's only interior quote is its first character */
		s->fields[i].pos_ = (c == '"') ? 1 : 0;
	}
}

/* Parse "HTTP/1.x NNN" once its first line has been collected */
static void parse_status(struct resp_scan *s)
{
	const char *p = memchr(s->status_line, ' ', s->status_len);
	int code = 0;

	if (p == NULL || strncmp(s->status_line, "HTTP/", 5) != 0) {
		return;
	}
	for (p++; p < s->status_line + s->status_len && *p >= '0' && *p <= '9'; p++) {
		code = code * 10 + (*p - '0');
	}
	s->status = code;
}

static void finish_value(struct resp_scan *s)
{
	struct resp_field *f = s->active;
	int64_t v = s->value;

	/* Pad up to the requested number of decimals */
	for (uint8_t d = s->fraction_digits; d < f->decimals; d++) {
		v *= 10;
	}
	f->value = s->negative ? -v : v;
	f->found = true;
}

/* Body byte handling */
static void feed_body_byte(struct resp_scan *s, char c)
{
	switch (s->state) {
	case SCAN_KEY:
		for (size_t i = 0; i < s->count; i++) {
			struct resp_field *f = &s->fields[i];
			size_t len = key_length(f);

			if (f->found) {
				continue;
			}
			if (c == pattern_char(f, f->pos_, len)) {
				if (++f->pos_ == len + 2) {
					s->state = SCAN_COLON;
					s->active = f;
					s->have_colon = false;
					return;
				}
			} else {
				f->pos_ = (c == '"') ? 1 : 0;
			}
		}
		return;

	case SCAN_COLON:
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			return;
		}
		if (c == ':' && !s->have_colon) {
			s->have_colon = true;
			return;
		}
		if (s->have_colon && (c == '-' || (c >= '0' && c <= '9'))) {
			s->state = SCAN_VALUE;
			s->have_digits = false;
			s->negative = false;
			s->in_fraction = false;
			s->fraction_digits = 0;
			s->value = 0;
			feed_body_byte(s, c);
			return;
		}
		/* Not a number (a string, null, ...): keep looking for the next match */
		restart_matching(s, c);
		return;

	case SCAN_VALUE:
		if (c == '-' && !s->have_digits && !s->negative && !s->in_fraction) {
			s->negative = true;
		} else if (c >= '0' && c <= '9') {
			s->have_digits = true;
			if (!s->in_fraction) {
				s->value = s->value * 10 + (c - '0');
			} else if (s->fraction_digits < s->active->decimals) {
				s->value = s->value * 10 + (c - '0');
				s->fraction_digits++;
			}
		} else if (c == '.' && s->have_digits && !s->in_fraction) {
			s->in_fraction = true;
		} else {
			/* Any other byte ends the number */
			if (s->have_digits) {
				finish_value(s);
			}
			restart_matching(s, c);
		}
		return;
	}
}

bool resp_scan_feed(struct resp_scan *s, const char *data, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		char c = data[i];

		if (!s->in_body) {
			if (s->status == 0 && s->status_len < sizeof(s->status_line) && c != '\r' &&
			    c != '\n') {
				s->status_line[s->status_len++] = c;
			} else if (s->status == 0 && s->status_len > 0) {
				parse_status(s);
				/* Keep a nonzero marker so a malformed line is not re-parsed */
				if (s->status == 0) {
					s->status = -1;
				}
			}

			if (c == header_end[s->header_match]) {
				if (++s->header_match == sizeof(header_end) - 1) {
					s->in_body = true;
				}
			} else {
				s->header_match = (c == '\r') ? 1 : 0;
			}
			continue;
		}

		feed_body_byte(s, c);
		if (all_found(s)) {
			return true;
		}
	}

	return all_found(s);
}

bool resp_scan_finish(struct resp_scan *s)
{
	if (s->state == SCAN_VALUE && s->have_digits) {
		finish_value(s);
		restart_matching(s, 0);
	}

	return all_found(s);
}

int resp_scan_status(const struct resp_scan *s)
{
	return s->status > 0 ? s->status : 0;
}
