#include "jalalix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failed = 0;

static void expect_str(const char *name, const char *got, const char *want)
{
	if (strcmp(got, want) != 0)
	{
		fprintf(stderr, "FAIL %s: got \"%s\" want \"%s\"\n", name, got, want);
		failed++;
	}
	else
	{
		printf("ok   %s\n", name);
	}
}

static void expect_int(const char *name, int got, int want)
{
	if (got != want)
	{
		fprintf(stderr, "FAIL %s: got %d want %d\n", name, got, want);
		failed++;
	}
	else
	{
		printf("ok   %s\n", name);
	}
}

static void expect_i64(const char *name, int64_t got, int64_t want)
{
	if (got != want)
	{
		fprintf(stderr, "FAIL %s: got %lld want %lld\n", name, (long long)got, (long long)want);
		failed++;
	}
	else
	{
		printf("ok   %s\n", name);
	}
}

int main(void)
{
	jalalix_t *j;
	jalalix_t *p;
	jalalix_t *a;
	jalalix_t *b;
	jalalix_t *c;
	char buf[256];
	char persian[64];
	char western[64];

	// Golden: 2025-02-25 -> 1403/12/07
	j = jalalix_create_iso("2025-02-25");

	if (!j)
	{
		fprintf(stderr, "FAIL create_iso returned NULL\n");
		return 1;
	}

	expect_i64("create_iso ms", jalalix_get_time(j), 1740441600000LL);

	if (jalalix_format(j, "YYYY/MM/DD", buf, sizeof(buf), 0, 0) < 0)
	{
		fprintf(stderr, "FAIL format\n");
		return 1;
	}

	expect_str("golden format YYYY/MM/DD", buf, "1403/12/07");
	expect_int("get_full_year", jalalix_get_full_year(j), 1403);
	expect_int("get_month", jalalix_get_month(j), 12);
	expect_int("get_date", jalalix_get_date(j), 7);
	expect_int("get_day", jalalix_get_day(j), 4);

	if (jalalix_to_iso_date(j, buf, sizeof(buf), 0) != 0)
	{
		fprintf(stderr, "FAIL to_iso_date gregorian\n");
		failed++;
	}
	else
	{
		// Local calendar date may differ near UTC midnight; for UTC+ offsets expect 2025-02-25
		printf("info to_iso_date gregorian = %s\n", buf);
	}

	if (jalalix_to_iso_date(j, buf, sizeof(buf), 1) != 0)
	{
		fprintf(stderr, "FAIL to_iso_date jalali\n");
		failed++;
	}
	else
	{
		expect_str("to_iso_date jalali", buf, "1403-12-07");
	}

	// parseJalali round-trip
	p = jalalix_parse_jalali("1403/12/07");
	if (!p)
	{
		fprintf(stderr, "FAIL parse_jalali NULL\n");
		failed++;
	}
	else
	{
		jalalix_format(p, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
		expect_str("parse_jalali format", buf, "1403/12/07");
		jalalix_to_iso_date(p, buf, sizeof(buf), 0);
		expect_str("parse_jalali to_iso_date", buf, "2025-02-25");
		jalalix_free(p);
	}

	// Persian digits
	if (jalalix_to_persian("1403/12/07", persian, sizeof(persian)) != 0)
	{
		fprintf(stderr, "FAIL to_persian\n");
		failed++;
	}
	else
	{
		expect_str("to_persian", persian, "۱۴۰۳/۱۲/۰۷");
	}
	if (jalalix_from_persian("۱۴۰۳/۱۲/۰۷", western, sizeof(western)) != 0)
	{
		fprintf(stderr, "FAIL from_persian\n");
		failed++;
	}
	else
	{
		expect_str("from_persian", western, "1403/12/07");
	}

	p = jalalix_parse_jalali("۱۴۰۳-۱۲-۰۷");
	if (!p)
	{
		fprintf(stderr, "FAIL parse persian digits NULL\n");
		failed++;
	}
	else
	{
		jalalix_format(p, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
		expect_str("parse persian digits", buf, "1403/12/07");
		jalalix_free(p);
	}

	jalalix_format(j, "YYYY/MM/DD", buf, sizeof(buf), 1, 0);
	expect_str("format persian_digits", buf, "۱۴۰۳/۱۲/۰۷");

	// clone / compare / add
	a = jalalix_clone(j);
	b = jalalix_create_iso("2025-02-26");
	expect_int("compare a<b", jalalix_compare(a, b), 1);
	expect_int("compare b<a", jalalix_compare(b, a), 0);

	c = jalalix_clone(j);
	jalalix_add_days(c, 1);
	jalalix_format(c, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
	expect_str("add_days 1", buf, "1403/12/08");

	jalalix_set_full_year(a, 1404, 0, 1);
	jalalix_format(a, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
	expect_str("set_full_year Farvardin 1", buf, "1404/01/01");

	jalalix_free(a);
	jalalix_free(b);
	jalalix_free(c);
	jalalix_free(j);

	// create_ms
	j = jalalix_create_ms(1740441600000LL);
	jalalix_format(j, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
	expect_str("create_ms golden", buf, "1403/12/07");
	jalalix_free(j);

	// Golden timezone: unixMs 1740430800000
	j = jalalix_create_ms(1740430800000LL);
	expect_int("set_timezone Tehran", jalalix_set_timezone(j, "Asia/Tehran"), 0);
	expect_str("get_timezone Tehran", jalalix_get_timezone(j), "Asia/Tehran");
	expect_i64("tehran instant preserved", jalalix_get_time(j), 1740430800000LL);
	jalalix_format(j, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
	expect_str("tehran jalali", buf, "1403/12/07");
	jalalix_format(j, "HH:mm:ss", buf, sizeof(buf), 0, 0);
	expect_str("tehran wall time", buf, "00:30:00");
	jalalix_to_iso_date(j, buf, sizeof(buf), 0);
	expect_str("tehran wall gregorian", buf, "2025-02-25");

	expect_int("set_timezone UTC", jalalix_set_timezone(j, "UTC"), 0);
	expect_i64("utc instant preserved", jalalix_get_time(j), 1740430800000LL);
	jalalix_format(j, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
	expect_str("utc jalali", buf, "1403/12/06");
	jalalix_format(j, "HH:mm:ss", buf, sizeof(buf), 0, 0);
	expect_str("utc wall time", buf, "21:00:00");
	jalalix_to_iso_date(j, buf, sizeof(buf), 0);
	expect_str("utc wall gregorian", buf, "2025-02-24");

	expect_int("alias utc", jalalix_set_timezone(j, "utc"), 0);
	expect_str("alias expect UTC", jalalix_get_timezone(j), "UTC");
	expect_int("invalid zone", jalalix_set_timezone(j, "Mars/Olympus"), -1);

	jalalix_free(j);

	j = jalalix_create_ms(1740484800000LL);
	jalalix_set_timezone(j, "Asia/Tehran");
	jalalix_format(j, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
	expect_str("utc noon tehran jalali", buf, "1403/12/07");
	jalalix_format(j, "HH:mm:ss", buf, sizeof(buf), 0, 0);
	expect_str("utc noon tehran wall", buf, "15:30:00");
	jalalix_free(j);

	printf("\ngolden parity\n");

	// calendar: utc_noon_create
	j = jalalix_create_ms(1740484800000LL);
	expect_int("golden utc_noon set UTC", jalalix_set_timezone(j, "UTC"), 0);
	jalalix_format(j, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
	expect_str("golden utc_noon format", buf, "1403/12/07");
	jalalix_to_iso_date(j, buf, sizeof(buf), 0);
	expect_str("golden utc_noon iso g", buf, "2025-02-25");
	jalalix_to_iso_date(j, buf, sizeof(buf), 1);
	expect_str("golden utc_noon iso j", buf, "1403-12-07");
	jalalix_free(j);

	// parse: dash
	p = jalalix_parse_jalali("1403-12-07");
	if (!p)
	{
		fprintf(stderr, "FAIL golden parse dash NULL\n");
		failed++;
	}
	else
	{
		jalalix_to_iso_date(p, buf, sizeof(buf), 0);
		expect_str("golden parse dash iso", buf, "2025-02-25");
		jalalix_free(p);
	}

	// format: persian digits + month name
	j = jalalix_create_iso("2025-02-25");
	jalalix_format(j, "DD MMM YYYY", buf, sizeof(buf), 1, 0);
	expect_str("golden format persian month", buf, "۰۷ اسفند ۱۴۰۳");
	jalalix_format(j, "YYYY-MM-DD", buf, sizeof(buf), 0, 1);
	expect_str("golden format gregorian", buf, "2025-02-25");

	// mutations: addMonths + getStartOfMonth
	jalalix_add_months(j, 1);
	jalalix_start_of_month(j);
	jalalix_format(j, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
	expect_str("golden add_months start", buf, "1404/01/01");

	// mutations: month quirk Farvardin
	jalalix_free(j);
	j = jalalix_create_iso("2025-02-25");
	jalalix_set_full_year(j, 1404, 0, 1);
	expect_int("golden month quirk getMonth", jalalix_get_month(j), 1);
	jalalix_format(j, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
	expect_str("golden month quirk format", buf, "1404/01/01");
	jalalix_free(j);

	// timezone: hours/minutes + jalaliIso (tehran offset instant)
	j = jalalix_create_ms(1740430800000LL);
	jalalix_set_timezone(j, "Asia/Tehran");
	jalalix_to_iso_date(j, buf, sizeof(buf), 1);
	expect_str("golden tehran jalaliIso", buf, "1403-12-07");
	expect_int("golden tehran hours", jalalix_get_hours(j), 0);
	expect_int("golden tehran minutes", jalalix_get_minutes(j), 30);

	jalalix_set_timezone(j, "UTC");
	jalalix_to_iso_date(j, buf, sizeof(buf), 1);
	expect_str("golden utc jalaliIso", buf, "1403-12-06");
	expect_int("golden utc hours", jalalix_get_hours(j), 21);
	expect_int("golden utc minutes", jalalix_get_minutes(j), 0);
	jalalix_free(j);

	// timezone: utc_noon_tehran hours/minutes
	j = jalalix_create_ms(1740484800000LL);
	jalalix_set_timezone(j, "Asia/Tehran");
	jalalix_to_iso_date(j, buf, sizeof(buf), 0);
	expect_str("golden noon tehran wallG", buf, "2025-02-25");
	expect_int("golden noon tehran hours", jalalix_get_hours(j), 15);
	expect_int("golden noon tehran minutes", jalalix_get_minutes(j), 30);
	jalalix_free(j);

	if (failed)
	{
		fprintf(stderr, "\n%d test(s) failed\n", failed);
		return 1;
	}

	printf("\nall tests passed\n");
	return 0;
}
