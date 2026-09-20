/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TC001_FORM_H_
#define TC001_FORM_H_

#include <stddef.h>

/**
 * Extract and URL-decode one field of an application/x-www-form-urlencoded body.
 *
 * @param body  NUL-terminated form body, e.g. "ssid=Home&password=a+b%21"
 * @param key   Field name
 * @param out   Output buffer, always NUL-terminated on success
 * @param len   Size of @p out
 * @return length of the decoded value, -ENOENT if the field is missing,
 *         -ENOSPC if it does not fit in @p out
 */
int form_get(const char *body, const char *key, char *out, size_t len);

/**
 * Append @p text to @p out at *@p pos with HTML special characters escaped.
 * Output is truncated (never overflows) and always NUL-terminated.
 */
void html_escape_append(char *out, size_t size, size_t *pos, const char *text);

#endif /* TC001_FORM_H_ */
