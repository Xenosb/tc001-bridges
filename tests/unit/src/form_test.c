/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include "form.h"

ZTEST(form, test_get_and_decode)
{
	char v[16];

	zassert_equal(form_get("ssid=Home+Net&password=p%40ss%21", "ssid", v, sizeof(v)), 8);
	zassert_ok(strcmp(v, "Home Net"));
	zassert_equal(form_get("ssid=Home+Net&password=p%40ss%21", "password", v, sizeof(v)), 5);
	zassert_ok(strcmp(v, "p@ss!"));
}

ZTEST(form, test_empty_and_missing)
{
	char v[16] = "junk";

	/* An empty password is valid and must come back as an empty string */
	zassert_equal(form_get("ssid=Home&password=", "password", v, sizeof(v)), 0);
	zassert_equal(v[0], '\0');
	zassert_equal(form_get("ssid=Home", "password", v, sizeof(v)), -ENOENT);
	/* Key must match whole name, not a suffix of another field */
	zassert_equal(form_get("xssid=a&ssid=b", "ssid", v, sizeof(v)), 1);
	zassert_equal(v[0], 'b');
}

ZTEST(form, test_bad_input)
{
	char v[4];

	zassert_equal(form_get("ssid=toolong", "ssid", v, sizeof(v)), -ENOSPC);
	zassert_equal(form_get("ssid=%zz", "ssid", v, sizeof(v)), -EINVAL);
	zassert_equal(form_get("ssid=%4", "ssid", v, sizeof(v)), -EINVAL);
}

ZTEST(form, test_html_escape)
{
	char out[32];
	size_t pos = 0;

	html_escape_append(out, sizeof(out), &pos, "a<b>&\"'");
	zassert_ok(strcmp(out, "a&lt;b&gt;&amp;&quot;&#39;"));

	/* Truncates rather than overflowing */
	char small[8];

	pos = 0;
	html_escape_append(small, sizeof(small), &pos, "<<<<<<");
	zassert_true(strlen(small) < sizeof(small));
}

ZTEST_SUITE(form, NULL, NULL, NULL, NULL, NULL);
