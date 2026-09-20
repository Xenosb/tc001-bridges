/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <errno.h>
#include <string.h>

#include "form.h"

static int hex_value(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

/* Decode the value starting at @p src, which ends at '&' or the end of the string. */
static int decode_value(const char *src, char *out, size_t len)
{
	size_t n = 0;

	while (*src != '\0' && *src != '&') {
		char c = *src++;

		if (c == '+') {
			c = ' ';
		} else if (c == '%') {
			int hi = hex_value(src[0]);
			int lo = hi < 0 ? -1 : hex_value(src[1]);

			if (hi < 0 || lo < 0) {
				return -EINVAL;
			}
			c = (char)(hi << 4 | lo);
			src += 2;
		}

		if (n + 1 >= len) {
			return -ENOSPC;
		}
		out[n++] = c;
	}
	out[n] = '\0';

	return n;
}

int form_get(const char *body, const char *key, char *out, size_t len)
{
	size_t key_len = strlen(key);
	const char *p = body;

	while (*p != '\0') {
		if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
			return decode_value(p + key_len + 1, out, len);
		}

		p = strchr(p, '&');
		if (p == NULL) {
			break;
		}
		p++;
	}

	return -ENOENT;
}

void html_escape_append(char *out, size_t size, size_t *pos, const char *text)
{
	for (; *text != '\0'; text++) {
		const char *rep;

		switch (*text) {
		case '&':
			rep = "&amp;";
			break;
		case '<':
			rep = "&lt;";
			break;
		case '>':
			rep = "&gt;";
			break;
		case '"':
			rep = "&quot;";
			break;
		case '\'':
			rep = "&#39;";
			break;
		default:
			rep = NULL;
			break;
		}

		size_t n = rep ? strlen(rep) : 1;

		if (*pos + n + 1 > size) {
			break;
		}
		if (rep) {
			memcpy(out + *pos, rep, n);
		} else {
			out[*pos] = *text;
		}
		*pos += n;
	}

	if (*pos < size) {
		out[*pos] = '\0';
	}
}
