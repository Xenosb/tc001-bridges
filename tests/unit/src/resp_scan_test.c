/*
 * Copyright (c) 2026 Bruno Vunderl
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string.h>

#include <zephyr/ztest.h>

#include "resp_scan.h"

#define HDR "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 99\r\n\r\n"

/* Feed `text` in pieces of `piece` bytes; true if every field was found */
static bool scan(struct resp_field *fields, size_t count, const char *text, size_t piece,
		 struct resp_scan *s)
{
	size_t len = strlen(text);

	resp_scan_init(s, fields, count);
	for (size_t i = 0; i < len; i += piece) {
		size_t n = MIN(piece, len - i);

		if (resp_scan_feed(s, text + i, n)) {
			return true;
		}
	}
	return resp_scan_finish(s);
}

/* One field, no decimals */
static bool scan1(const char *key, const char *text, size_t piece, struct resp_scan *s,
		  int64_t *value)
{
	struct resp_field f = {.key = key};
	bool ok = scan(&f, 1, text, piece, s);

	*value = f.value;
	return ok;
}

ZTEST(resp_scan, test_compact_and_spaced)
{
	struct resp_scan s;
	int64_t v;

	zassert_true(scan1("downloads", HDR "{\"crate\":{\"downloads\":2252,\"x\":1}}", 4096, &s, &v));
	zassert_equal(v, 2252);
	zassert_equal(resp_scan_status(&s), 200);

	/* GitHub-style pretty printing */
	zassert_true(scan1("stargazers_count", HDR "{\n  \"stargazers_count\": 248,\n}", 4096, &s, &v));
	zassert_equal(v, 248);
}

ZTEST(resp_scan, test_split_at_every_boundary)
{
	const char *doc = HDR "{\"a\":1,\"totalHits\":1,\"data\":[{\"totalDownloads\": 968}]}";
	struct resp_scan s;
	int64_t v;

	/* One byte at a time, plus every other piece size, exercises every split point */
	for (size_t piece = 1; piece < strlen(doc); piece++) {
		zassert_true(scan1("totalDownloads", doc, piece, &s, &v), "piece %zu", piece);
		zassert_equal(v, 968, "piece %zu", piece);
		zassert_equal(resp_scan_status(&s), 200, "piece %zu", piece);
	}
}

ZTEST(resp_scan, test_first_match_and_lookalikes)
{
	struct resp_scan s;
	int64_t v;

	/* "recent_downloads" and "version_downloads" must not match "downloads",
	 * and a string value must not stop the search
	 */
	zassert_true(scan1("downloads",
			   HDR "{\"recent_downloads\":5,\"version_downloads\":6,"
			       "\"downloads\":\"n/a\",\"downloads\":77,\"downloads\":88}",
			   1, &s, &v));
	zassert_equal(v, 77);
}

ZTEST(resp_scan, test_number_at_end_of_stream)
{
	struct resp_scan s;
	int64_t v;

	/* No terminator after the digits: only resp_scan_finish() can complete it */
	zassert_true(scan1("n", HDR "{\"n\":1234", 3, &s, &v));
	zassert_equal(v, 1234);
}

ZTEST(resp_scan, test_key_in_headers_ignored)
{
	struct resp_scan s;
	int64_t v;

	zassert_true(scan1("n", "HTTP/1.0 200 OK\r\nX-Note: \"n\":9\r\n\r\n{\"n\":3}", 2, &s, &v));
	zassert_equal(v, 3);
}

ZTEST(resp_scan, test_status_and_missing)
{
	struct resp_scan s;
	int64_t v;

	zassert_false(scan1("n", "HTTP/1.1 403 Forbidden\r\n\r\n{\"message\":\"rate limit\"}", 5, &s, &v));
	zassert_equal(resp_scan_status(&s), 403);

	zassert_false(scan1("n", HDR "{\"n\":null,\"n\":\"x\"}", 7, &s, &v));
	zassert_false(scan1("n", "garbage with no headers", 5, &s, &v));
	zassert_equal(resp_scan_status(&s), 0);
}

ZTEST(resp_scan, test_signed_and_decimals)
{
	struct resp_field f = {.key = "t", .decimals = 1};
	struct resp_scan s;

	zassert_true(scan(&f, 1, HDR "{\"t\":14.3}", 3, &s));
	zassert_equal(f.value, 143);
	zassert_true(scan(&f, 1, HDR "{\"t\":-3.46,\"x\":1}", 2, &s));
	zassert_equal(f.value, -34, "extra digits are dropped");
	zassert_true(scan(&f, 1, HDR "{\"t\":-0.5}", 1, &s));
	zassert_equal(f.value, -5, "a negative that starts with 0");
	zassert_true(scan(&f, 1, HDR "{\"t\":7}", 1, &s));
	zassert_equal(f.value, 70, "no decimals in the source pads up");
	zassert_true(scan(&f, 1, HDR "{\"t\":-12}", 4, &s));
	zassert_equal(f.value, -120);

	/* Coordinates with four decimals, as the location lookup returns them */
	struct resp_field lat = {.key = "latitude", .decimals = 4};

	zassert_true(scan(&lat, 1, HDR "{\"latitude\":52.5243699}", 5, &s));
	zassert_equal(lat.value, 525243);
	zassert_true(scan(&lat, 1, HDR "{\"latitude\":-33.86882}", 5, &s));
	zassert_equal(lat.value, -338688);
}

ZTEST(resp_scan, test_a_lone_minus_or_point_is_not_a_number)
{
	struct resp_field f = {.key = "t", .decimals = 1};
	struct resp_scan s;

	zassert_false(scan(&f, 1, HDR "{\"t\":-}", 1, &s));
	zassert_false(scan(&f, 1, HDR "{\"t\":.5}", 1, &s));
	zassert_false(scan(&f, 1, HDR "{\"t\":-\"x\"}", 1, &s));
}

/* Open-Meteo's real answer: the keys also appear as strings in current_units, before the values */
ZTEST(resp_scan, test_several_fields_from_one_response)
{
	const char *doc =
		HDR "{\"latitude\":52.52,\"utc_offset_seconds\":7200,\"timezone\":\"Europe/Berlin\","
		    "\"current_units\":{\"time\":\"iso8601\",\"temperature_2m\":\"\xc2\xb0""C\","
		    "\"weather_code\":\"wmo code\",\"is_day\":\"\"},"
		    "\"current\":{\"time\":\"2026-09-20T22:15\",\"interval\":900,"
		    "\"temperature_2m\":-3.4,\"weather_code\":61,\"is_day\":0}}";

	for (size_t piece = 1; piece < 40; piece += 3) {
		struct resp_field f[] = {
			{.key = "utc_offset_seconds"},
			{.key = "temperature_2m", .decimals = 1},
			{.key = "weather_code"},
			{.key = "is_day"},
		};
		struct resp_scan s;

		zassert_true(scan(f, ARRAY_SIZE(f), doc, piece, &s), "piece %zu", piece);
		zassert_equal(f[0].value, 7200, "piece %zu", piece);
		zassert_equal(f[1].value, -34, "piece %zu", piece);
		zassert_equal(f[2].value, 61, "piece %zu", piece);
		zassert_equal(f[3].value, 0, "piece %zu", piece);
	}
}

ZTEST(resp_scan, test_some_fields_missing)
{
	struct resp_field f[] = {{.key = "a"}, {.key = "b"}};
	struct resp_scan s;

	zassert_false(scan(f, 2, HDR "{\"a\":1}", 2, &s));
	zassert_true(f[0].found);
	zassert_false(f[1].found);
}

ZTEST_SUITE(resp_scan, NULL, NULL, NULL, NULL, NULL);
