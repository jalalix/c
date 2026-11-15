/**
 * JalaliX - pure C99 port of the TypeScript algorithm.
 * Base date: 1348/10/11 ↔ 1970-01-01; leapYearDivision = 0.24219858156
 */
#include "jalalix.h"
#include "tz.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define JX_DATE_LENGTH 86400000LL
#define JX_FIRST_DAY_OF_WEEK 6
#define JX_LEAP_DIV 0.24219858156
#define JX_BASE_YEAR 1348
#define JX_BASE_MONTH 10
#define JX_BASE_DAY 11
#define JX_TZ_ID_MAX 128

struct jalalix
{
	int64_t ms;
	char timezone[JX_TZ_ID_MAX];
	int year;
	int month;
	int day;
	int hours;
	int min;
	int sec;
	int ms_part;
	int is_leap;
	int converted;
};

static const char *const WEEK_DAYS_NAMES[] = {"شنبه", "یکشنبه", "دوشنبه", "سه شنبه", "چهار شنبه", "پنج شنبه", "جمعه"};
static const char *const WEEK_DAYS_LETTERS[] = {"ش", "ی", "د", "س", "چ", "پ", "ج"};
static const char *const WEEK_DAYS_GREG_NAMES[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *const WEEK_DAYS_GREG_LETTERS[] = {"S", "M", "T", "W", "T", "F", "S"};
static const char *const MONTHS_NAMES[] = {"فروردین", "اردیبهشت", "خرداد", "تیر", "مرداد", "شهریور", "مهر", "آبان", "آذر", "دی", "بهمن", "اسفند"};
static const char *const MONTHS_GREG_NAMES[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
static const char *const PERSIAN_DIGITS[] = {"۰", "۱", "۲", "۳", "۴", "۵", "۶", "۷", "۸", "۹"};
static const char *const AM = "ق.ظ.";
static const char *const PM = "ب.ظ.";
static const char *const AM_GREG = "AM";
static const char *const PM_GREG = "PM";

// Days since Unix epoch for civil Gregorian Y-M-D (UTC date). Howard Hinnant algorithm.
static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
	y -= m <= 2;
	{
		const int64_t era = (y >= 0 ? y : y - 399) / 400;
		const unsigned yoe = (unsigned)(y - era * 400);
		const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
		const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		return era * 146097 + (int64_t)doe - 719468;
	}
}

static void civil_from_days(int64_t z, int *y_out, unsigned *m_out, unsigned *d_out)
{
	z += 719468;
	{
		const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
		const unsigned doe = (unsigned)(z - era * 146097);
		const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		const int y = (int)(yoe) + (int)(era * 400);
		const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		const unsigned mp = (5 * doy + 2) / 153;
		const unsigned d = doy - (153 * mp + 2) / 5 + 1;
		const unsigned m = mp < 10 ? mp + 3 : mp - 9;
		*y_out = y + (m <= 2);
		*m_out = m;
		*d_out = d;
	}
}

static void ensure_jalali(jalalix_t *j);
static void sync_time_parts(jalalix_t *j);

static int64_t wall_ms_of(const jalalix_t *j)
{
	int64_t wall;

	if (!j)
		return 0;

	if (jx_tz_utc_to_wall_ms(j->timezone, j->ms, &wall) != 0)
		return j->ms;

	return wall;
}

// Euclidean remainder (non-negative).
static int64_t rem_euclid(int64_t a, int64_t b)
{
	int64_t r = a % b;

	return r < 0 ? r + b : r;
}

static int64_t div_euclid(int64_t a, int64_t b)
{
	int64_t q = a / b;
	int64_t r = a % b;

	if (r < 0)
		q -= 1;

	return q;
}

// Split civil (as-if-UTC) milliseconds into Gregorian parts.
static void ms_to_civil(int64_t ms, int *y_out, unsigned *m_out, unsigned *d_out, int *h_out,
						int *mi_out, int *s_out, int *ms_part)
{
	int64_t total_secs = div_euclid(ms, 1000);
	int64_t total_mins = div_euclid(total_secs, 60);
	int64_t total_hours = div_euclid(total_mins, 60);
	int64_t days = div_euclid(total_hours, 24);
	*ms_part = (int)rem_euclid(ms, 1000);
	*s_out = (int)rem_euclid(total_secs, 60);
	*mi_out = (int)rem_euclid(total_mins, 60);
	*h_out = (int)rem_euclid(total_hours, 24);
	civil_from_days(days, y_out, m_out, d_out);
}

static int check_leap_year(int year)
{
	if (year > 0)
	{
		double check = (year + 2346) * JX_LEAP_DIV;
		return (check - floor(check)) < JX_LEAP_DIV;
	}

	return 0;
}

// Match TypeScript reset(): clear, then immediately toJalali().
static void jalali_reset(jalalix_t *j)
{
	j->year = JX_BASE_YEAR;
	j->month = JX_BASE_MONTH;
	j->day = JX_BASE_DAY;
	j->converted = 0;

	ensure_jalali(j);
}

static void advance_jalali_one_day(int *year, int *month, int *day)
{
	(*day)++;

	if (*month < 7 && *day > 31)
	{
		(*month)++;
		*day = 1;
	}
	else if (*month > 6 && *month < 12 && *day > 30)
	{
		(*month)++;
		*day = 1;
	}
	else if (*month == 12)
	{
		int leap = check_leap_year(*year);
		if (leap && *day > 30)
		{
			(*year)++;
			*month = 1;
			*day = 1;
		}
		else if (!leap && *day > 29)
		{
			(*year)++;
			*month = 1;
			*day = 1;
		}
	}
}

static void retreat_jalali_one_day(int *year, int *month, int *day)
{
	(*day)--;

	if (*day < 1)
	{
		if (*month == 1)
		{
			(*year)--;
			*month = 12;
			*day = check_leap_year(*year) ? 30 : 29;
		}
		else if (*month < 8)
		{
			(*month)--;
			*day = 31;
		}
		else if (*month < 13)
		{
			(*month)--;
			*day = 30;
		}
	}
}

static int64_t floor_div_ms(int64_t ms)
{
	int64_t diff_days = ms / JX_DATE_LENGTH;

	if (ms < 0 && (ms % JX_DATE_LENGTH) != 0)
		diff_days--;

	return diff_days;
}

static void ensure_jalali(jalalix_t *j)
{
	int64_t diff_days;
	int64_t i;
	int64_t wall;

	if (j->converted)
		return;

	wall = wall_ms_of(j);
	diff_days = floor_div_ms(wall);
	j->year = JX_BASE_YEAR;
	j->month = JX_BASE_MONTH;
	j->day = JX_BASE_DAY;

	if (diff_days > 0)
	{
		for (i = 0; i < diff_days; i++)
			advance_jalali_one_day(&j->year, &j->month, &j->day);
	}
	else if (diff_days < 0)
	{
		for (i = 0; i < -diff_days; i++)
			retreat_jalali_one_day(&j->year, &j->month, &j->day);
	}

	j->is_leap = check_leap_year(j->year);
	j->converted = 1;
	sync_time_parts(j);
}

static void ensure_jalali_const(const jalalix_t *j)
{
	ensure_jalali((jalalix_t *)j);
}

static int gregorian_weekday_wall(const jalalix_t *j)
{
	// 0=Sun … 6=Sat from wall civil day (epoch Thursday = 4).
	int64_t days = floor_div_ms(wall_ms_of(j));
	int w = (int)((days + 4) % 7);

	if (w < 0)
		w += 7;

	return w;
}

static int64_t to_gregorian_ms(const jalalix_t *j, int year, int month, int day)
{
	int cy = JX_BASE_YEAR, cm = JX_BASE_MONTH, cd = JX_BASE_DAY;
	int64_t diff_days = 0;
	int hours, min, sec, ms_part;
	int y;
	unsigned mo, d;
	int64_t wall_as_utc;
	int64_t utc;

	{
		int hy;
		unsigned hmo, hd;
		ms_to_civil(wall_ms_of(j), &hy, &hmo, &hd, &hours, &min, &sec, &ms_part);
		(void)hy;
		(void)hmo;
		(void)hd;
	}

	if (year > 1348 || (year == 1348 && month > 10) || (year == 1348 && month == 10 && day >= 11))
	{
		for (;;)
		{
			if (cy == year && cm == month && cd == day)
				break;

			diff_days++;
			advance_jalali_one_day(&cy, &cm, &cd);
		}
	}
	else if (year < 1348 || (year == 1348 && month < 10) || (year == 1348 && month == 10 && day < 11))
	{
		for (;;)
		{
			if (cy == year && cm == month && cd == day)
				break;

			diff_days--;
			retreat_jalali_one_day(&cy, &cm, &cd);
		}
	}

	wall_as_utc =
		diff_days * JX_DATE_LENGTH + (int64_t)(hours * 3600 + min * 60 + sec) * 1000 + ms_part;
	ms_to_civil(wall_as_utc, &y, &mo, &d, &hours, &min, &sec, &ms_part);

	if (jx_tz_wall_to_utc_ms(j->timezone, y, (int)mo, (int)d, hours, min, sec, ms_part, &utc) != 0)
		return wall_as_utc;

	return utc;
}

static int64_t normalize_jalali(jalalix_t *j, int year, int month, int day)
{
	if (month < 1)
	{
		int year_diff = (int)ceil((1.0 - month) / 12.0);
		year -= year_diff;
		month = 12 * year_diff + month;
	}
	else if (month > 12)
	{
		int year_diff = (month - 1) / 12;
		year += year_diff;
		month = month - 12 * year_diff;
	}

	if (month > 6 && month < 12 && day > 30)
		day = 30;
	else if (month == 12)
	{
		int leap = check_leap_year(year);
		if (leap && day > 30)
			day = 30;
		else if (!leap && day > 29)
			day = 29;
	}

	j->ms = to_gregorian_ms(j, year, month, day);
	jalali_reset(j);

	return j->ms;
}

static void sync_time_parts(jalalix_t *j)
{
	int y;
	unsigned m, d;
	ms_to_civil(wall_ms_of(j), &y, &m, &d, &j->hours, &j->min, &j->sec, &j->ms_part);
	(void)y;
	(void)m;
	(void)d;
}

static jalalix_t *jalalix_alloc(int64_t ms)
{
	jalalix_t *j = (jalalix_t *)calloc(1, sizeof(jalalix_t));

	if (!j)
		return NULL;

	j->ms = ms;
	snprintf(j->timezone, sizeof(j->timezone), "UTC");
	jalali_reset(j);

	return j;
}

static void add_zero(char *buf, size_t buflen, int number)
{
	if (number > -1 && number < 10)
		snprintf(buf, buflen, "0%d", number);
	else
		snprintf(buf, buflen, "%d", number);
}

static int get_day_of_year_jalali(const jalalix_t *j)
{
	int output;
	ensure_jalali_const(j);
	output = j->day;

	if (j->month < 7)
		output += (j->month - 1) * 31;
	else if (j->month < 12)
		output += 186 + (j->month - 7) * 30;
	else
		output += 336;

	return output;
}

static int get_day_jalali(const jalalix_t *j)
{
	return (gregorian_weekday_wall(j) - JX_FIRST_DAY_OF_WEEK + 7 + 1) % 7;
}

static int get_day_iso(const jalalix_t *j)
{
	return ((get_day_jalali(j) + 6) % 7) + 1;
}

static int get_week_of_year_jalali(const jalalix_t *j)
{
	jalalix_t *tmp;
	int day_of_year, day_of_week, week;

	day_of_year = get_day_of_year_jalali(j);
	tmp = jalalix_clone(j);

	if (!tmp)
		return 0;

	jalalix_start_of_year(tmp);
	day_of_week = get_day_iso(tmp);
	jalalix_free(tmp);
	week = (int)ceil((day_of_year + day_of_week - 1) / 7.0);

	return week;
}

static int is_leap_gregorian(int y)
{
	return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int get_gregorian_day_of_year(const jalalix_t *j)
{
	static const int CUM[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
	int y, h, mi, s, msp;
	unsigned m, d;
	int n;
	ms_to_civil(wall_ms_of(j), &y, &m, &d, &h, &mi, &s, &msp);
	(void)h;
	(void)mi;
	(void)s;
	(void)msp;
	n = CUM[m - 1] + (int)d;

	if (m > 2 && is_leap_gregorian(y))
		n += 1;

	return n;
}

static int get_gregorian_week_of_year(const jalalix_t *j)
{
	int y, h, mi, s, msp;
	unsigned m, d;
	int64_t utc_days;
	int64_t adj;
	int iso_y;
	unsigned iso_m, iso_d;
	int64_t year_start;
	double days;
	int js_dow;

	ms_to_civil(wall_ms_of(j), &y, &m, &d, &h, &mi, &s, &msp);

	(void)h;
	(void)mi;
	(void)s;
	(void)msp;
	utc_days = days_from_civil(y, m, d);
	js_dow = (int)((utc_days + 4) % 7);

	if (js_dow < 0)
		js_dow += 7;
	adj = 4 - (js_dow == 0 ? 7 : js_dow);
	civil_from_days(utc_days + adj, &iso_y, &iso_m, &iso_d);
	year_start = days_from_civil(iso_y, 1, 1);
	days = (double)((utc_days + adj) - year_start) + 1.0;
	(void)iso_m;
	(void)iso_d;

	return (int)ceil(days / 7.0);
}

static int set_wall_hms(jalalix_t *j, int hours, int min, int sec, int ms)
{
	int y, h, mi, s, msp;
	unsigned mo, d;
	int64_t utc;

	ms_to_civil(wall_ms_of(j), &y, &mo, &d, &h, &mi, &s, &msp);
	(void)h;
	(void)mi;
	(void)s;
	(void)msp;

	if (ms < 0)
		ms = 0;

	ms %= 1000;

	if (jx_tz_wall_to_utc_ms(j->timezone, y, (int)mo, (int)d, hours, min, sec, ms, &utc) != 0)
		return -1;

	j->ms = utc;
	j->converted = 0;
	jalali_reset(j);

	return 0;
}

static int normalize_date_days(jalalix_t *j, int delta_days)
{
	j->ms += (int64_t)delta_days * JX_DATE_LENGTH;
	j->converted = 0;
	jalali_reset(j);

	return 0;
}

jalalix_t *jalalix_create_now(void)
{
	return jalalix_alloc((int64_t)time(NULL) * 1000);
}

jalalix_t *jalalix_create_ms(int64_t ms)
{
	return jalalix_alloc(ms);
}

jalalix_t *jalalix_create_iso(const char *iso)
{
	int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0, msec = 0;
	int n;
	int has_time = 0;
	int is_utc = 0;
	const char *p;
	struct tm tm;
	time_t t;

	if (!iso)
		return NULL;

	n = sscanf(iso, "%d-%d-%d", &y, &mo, &d);

	if (n != 3 || mo < 1 || mo > 12 || d < 1 || d > 31)
		return NULL;

	p = iso;

	while (*p && *p != 'T' && *p != ' ')
		p++;

	if (*p == 'T' || *p == ' ')
	{
		has_time = 1;
		p++;

		if (sscanf(p, "%d:%d:%d", &h, &mi, &s) >= 2)
		{
			const char *dot = strchr(p, '.');

			if (dot)
			{
				msec = atoi(dot + 1);

				while (msec > 999)
					msec /= 10;
			}
		}

		if (strchr(p, 'Z') != NULL)
			is_utc = 1;
	}

	if (!has_time)
	{
		// Date-only ISO -> UTC midnight (ECMAScript).
		return jalalix_alloc(days_from_civil(y, (unsigned)mo, (unsigned)d) * JX_DATE_LENGTH);
	}

	// Timed ISO without offset (or with Z) -> UTC instant.
	{
		int64_t ms = days_from_civil(y, (unsigned)mo, (unsigned)d) * JX_DATE_LENGTH;
		ms += (int64_t)(h * 3600 + mi * 60 + s) * 1000 + msec;
		(void)is_utc;
		(void)tm;
		(void)t;

		return jalalix_alloc(ms);
	}
}

void jalalix_free(jalalix_t *j)
{
	free(j);
}

jalalix_t *jalalix_parse_jalali(const char *value)
{
	char normalized[256];
	int year, month, day, hours = 0, minutes = 0, seconds = 0;
	int n;
	char sep1, sep2;
	jalalix_t *out;
	char *rest;
	char *s;
	char *e;

	if (!value)
		return NULL;

	if (jalalix_from_persian(value, normalized, sizeof(normalized)) != 0)
		return NULL;

	s = normalized;

	while (*s && isspace((unsigned char)*s))
		s++;

	if (s != normalized)
		memmove(normalized, s, strlen(s) + 1);

	e = normalized + strlen(normalized);

	while (e > normalized && isspace((unsigned char)e[-1]))
		*--e = '\0';

	n = sscanf(normalized, "%d%c%d%c%d", &year, &sep1, &month, &sep2, &day);

	if (n != 5 || (sep1 != '/' && sep1 != '-' && sep1 != '.') || sep1 != sep2)
		return NULL;

	if (month < 1 || month > 12 || day < 1 || day > 31)
		return NULL;

	rest = normalized;

	while (*rest && *rest != ' ' && *rest != 'T')
		rest++;

	if (*rest == ' ' || *rest == 'T')
	{
		rest++;
		sscanf(rest, "%d:%d:%d", &hours, &minutes, &seconds);
	}

	out = jalalix_create_now();

	if (!out)
		return NULL;

	jalalix_set_full_year(out, year, month - 1, day);
	jalalix_set_hours(out, hours, minutes, seconds, 0);

	return out;
}

int jalalix_get_full_year(const jalalix_t *j)
{
	if (!j)
		return 0;

	ensure_jalali_const(j);

	return j->year;
}

int jalalix_get_month(const jalalix_t *j)
{
	if (!j)
		return 0;

	ensure_jalali_const(j);

	return j->month;
}

int jalalix_get_date(const jalalix_t *j)
{
	if (!j)
		return 0;

	ensure_jalali_const(j);

	return j->day;
}

int jalalix_get_day(const jalalix_t *j)
{
	if (!j)
		return 0;

	return get_day_jalali(j);
}

int jalalix_get_hours(const jalalix_t *j)
{
	if (!j)
		return 0;

	ensure_jalali_const(j);

	return j->hours;
}

int jalalix_get_minutes(const jalalix_t *j)
{
	if (!j)
		return 0;

	ensure_jalali_const(j);

	return j->min;
}

int jalalix_get_seconds(const jalalix_t *j)
{
	if (!j)
		return 0;

	ensure_jalali_const(j);

	return j->sec;
}

int jalalix_get_milliseconds(const jalalix_t *j)
{
	if (!j)
		return 0;

	ensure_jalali_const(j);

	return j->ms_part;
}

int64_t jalalix_get_time(const jalalix_t *j)
{
	return j ? j->ms : 0;
}

int64_t jalalix_set_time(jalalix_t *j, int64_t ms)
{
	if (!j)
		return 0;

	j->ms = ms;
	jalali_reset(j);
	sync_time_parts(j);

	return j->ms;
}

int64_t jalalix_set_hours(jalalix_t *j, int hours, int min, int sec, int ms)
{
	if (!j)
		return 0;

	set_wall_hms(j, hours, min, sec, ms);

	return j->ms;
}

int64_t jalalix_set_date(jalalix_t *j, int date)
{
	int jday;

	if (!j)
		return 0;

	ensure_jalali(j);
	jday = j->day;
	normalize_date_days(j, date - jday);
	jalali_reset(j);

	return j->ms;
}

int64_t jalalix_set_month(jalalix_t *j, int month, int date)
{
	if (!j)
		return 0;

	ensure_jalali(j);
	normalize_jalali(j, j->year, month + 1, j->day);
	jalali_reset(j);

	if (date >= 0)
	{
		ensure_jalali(j);
		normalize_date_days(j, date - j->day);
		jalali_reset(j);
	}

	return j->ms;
}

int64_t jalalix_set_full_year(jalalix_t *j, int year, int month, int date)
{
	int mon;

	if (!j)
		return 0;

	ensure_jalali(j);
	mon = month >= 0 ? month + 1 : j->month;
	normalize_jalali(j, year, mon, j->day);
	jalali_reset(j);

	if (date >= 0)
	{
		ensure_jalali(j);
		normalize_date_days(j, date - j->day);
		jalali_reset(j);
	}

	return j->ms;
}

jalalix_t *jalalix_add_years(jalalix_t *j, int years)
{
	if (!j)
		return NULL;

	ensure_jalali(j);
	normalize_jalali(j, j->year + years, j->month, j->day);
	jalali_reset(j);

	return j;
}

jalalix_t *jalalix_add_months(jalalix_t *j, int months)
{
	if (!j)
		return NULL;

	ensure_jalali(j);
	normalize_jalali(j, j->year, j->month + months, j->day);
	jalali_reset(j);

	return j;
}

jalalix_t *jalalix_add_weeks(jalalix_t *j, int weeks)
{
	if (!j)
		return NULL;

	normalize_date_days(j, weeks * 7);
	jalali_reset(j);

	return j;
}

jalalix_t *jalalix_add_days(jalalix_t *j, int days)
{
	if (!j)
		return NULL;

	normalize_date_days(j, days);
	jalali_reset(j);

	return j;
}

jalalix_t *jalalix_start_of_day(jalalix_t *j)
{
	if (!j)
		return NULL;

	set_wall_hms(j, 0, 0, 0, 0);

	return j;
}

jalalix_t *jalalix_end_of_day(jalalix_t *j)
{
	if (!j)
		return NULL;

	set_wall_hms(j, 23, 59, 59, 999);

	return j;
}

jalalix_t *jalalix_start_of_year(jalalix_t *j)
{
	if (!j)
		return NULL;

	return jalalix_start_of_day(jalalix_add_days(j, -get_day_of_year_jalali(j) + 1));
}

jalalix_t *jalalix_start_of_month(jalalix_t *j)
{
	if (!j)
		return NULL;

	ensure_jalali(j);

	return jalalix_start_of_day(jalalix_add_days(j, -j->day + 1));
}

jalalix_t *jalalix_start_of_week(jalalix_t *j)
{
	if (!j)
		return NULL;

	return jalalix_start_of_day(jalalix_add_days(j, -get_day_iso(j) + 1));
}

jalalix_t *jalalix_end_of_year(jalalix_t *j)
{
	int last_day;

	if (!j)
		return NULL;

	ensure_jalali(j);
	last_day = j->is_leap ? 366 : 365;

	return jalalix_end_of_day(jalalix_add_days(j, last_day - get_day_of_year_jalali(j)));
}

jalalix_t *jalalix_end_of_month(jalalix_t *j)
{
	int last_day = 31;

	if (!j)
		return NULL;

	ensure_jalali(j);

	if (j->month > 6 && j->month < 12)
		last_day = 30;
	else if (j->month == 12)
		last_day = j->is_leap ? 30 : 29;

	return jalalix_end_of_day(jalalix_add_days(j, last_day - j->day));
}

jalalix_t *jalalix_end_of_week(jalalix_t *j)
{
	if (!j)
		return NULL;

	return jalalix_end_of_day(jalalix_add_days(j, 7 - get_day_iso(j)));
}

jalalix_t *jalalix_clone(const jalalix_t *j)
{
	jalalix_t *c;

	if (!j)
		return NULL;

	c = jalalix_alloc(j->ms);

	if (!c)
		return NULL;

	snprintf(c->timezone, sizeof(c->timezone), "%s", j->timezone);
	c->converted = 0;
	jalali_reset(c);

	return c;
}

int jalalix_set_timezone(jalalix_t *j, const char *tz)
{
	char resolved[JX_TZ_ID_MAX];

	if (!j || !tz)
		return -1;

	if (jx_tz_resolve(tz, j->timezone, resolved, sizeof(resolved)) != 0)
		return -1;
	snprintf(j->timezone, sizeof(j->timezone), "%s", resolved);
	j->converted = 0;
	jalali_reset(j);

	return 0;
}

const char *jalalix_get_timezone(const jalalix_t *j)
{
	return j ? j->timezone : NULL;
}

int jalalix_to_iso_date(const jalalix_t *j, char *out, size_t out_len, int jalali)
{
	if (!j || !out || out_len < 11)
		return -1;

	if (jalali)
	{
		char tmp[32];
		if (jalalix_format(j, "YYYY-MM-DD", tmp, sizeof(tmp), 0, 0) < 0)
			return -1;
		snprintf(out, out_len, "%s", tmp);
		return 0;
	}
	else
	{
		int y, h, mi, s, msp;
		unsigned m, d;
		ms_to_civil(wall_ms_of(j), &y, &m, &d, &h, &mi, &s, &msp);
		(void)h;
		(void)mi;
		(void)s;
		(void)msp;
		snprintf(out, out_len, "%04d-%02d-%02d", y, (int)m, (int)d);
		return 0;
	}
}

int jalalix_to_persian(const char *in, char *out, size_t out_len)
{
	size_t oi = 0;
	const unsigned char *p;

	if (!in || !out || out_len == 0)
		return -1;
	p = (const unsigned char *)in;
	while (*p)
	{
		if (*p >= '0' && *p <= '9')
		{
			const char *pd = PERSIAN_DIGITS[*p - '0'];
			size_t len = strlen(pd);
			if (oi + len >= out_len)
				return -1;
			memcpy(out + oi, pd, len);
			oi += len;
			p++;
		}
		else
		{
			if (oi + 1 >= out_len)
				return -1;
			out[oi++] = (char)*p++;
		}
	}
	out[oi] = '\0';

	return 0;
}

int jalalix_from_persian(const char *in, char *out, size_t out_len)
{
	size_t oi = 0;
	const unsigned char *p;

	if (!in || !out || out_len == 0)
		return -1;

	p = (const unsigned char *)in;

	while (*p)
	{
		int digit = -1;
		int i;

		if (p[0] == 0xDB && p[1])
		{
			for (i = 0; i <= 9; i++)
			{
				const unsigned char *pd = (const unsigned char *)PERSIAN_DIGITS[i];
				if (p[0] == pd[0] && p[1] == pd[1])
				{
					digit = i;
					break;
				}
			}
		}
		else if (p[0] == 0xD9 && p[1] >= 0xA0 && p[1] <= 0xA9)
		{
			digit = (int)(p[1] - 0xA0);
		}

		if (digit >= 0)
		{
			if (oi + 1 >= out_len)
				return -1;
			out[oi++] = (char)('0' + digit);
			p += 2;
		}
		else
		{
			if (oi + 1 >= out_len)
				return -1;
			out[oi++] = (char)*p++;
		}
	}

	out[oi] = '\0';

	return 0;
}

static int is_valid_jalali_civil_date(int year, int month, int day)
{
	if (year < 1 || month < 1 || month > 12 || day < 1)
		return 0;

	if (month <= 6)
		return day <= 31;

	if (month <= 11)
		return day <= 30;

	return day <= (check_leap_year(year) ? 30 : 29);
}

static int is_valid_gregorian_civil_date(int year, int month, int day)
{
	int y2;
	unsigned mo, da;

	if (month < 1 || month > 12 || day < 1)
		return 0;

	civil_from_days(days_from_civil(year, (unsigned)month, (unsigned)day), &y2, &mo, &da);

	return y2 == year && (int)mo == month && (int)da == day;
}

static int write_query_out(char *out, size_t out_len, const char *s)
{
	size_t n;

	if (!out || out_len == 0)
		return -1;

	if (!s)
		s = "";
	n = strlen(s);

	if (n >= out_len)
		return -1;

	memcpy(out, s, n + 1);

	return 0;
}

static int contains_nan_ci(const char *s)
{
	size_t i;
	size_t len;

	if (!s)
		return 0;

	len = strlen(s);

	for (i = 0; i + 2 < len; i++)
	{
		if ((s[i] == 'n' || s[i] == 'N') && (s[i + 1] == 'a' || s[i + 1] == 'A') &&
			(s[i + 2] == 'n' || s[i + 2] == 'N'))
			return 1;
	}

	return 0;
}

static int is_query_sentinel(const char *value)
{
	if (!value)
		return 1;

	if (strcmp(value, "null") == 0)
		return 1;

	if (value[0] == '\0')
		return 1;

	if (strcmp(value, "undefined") == 0)
		return 1;

	if (strcasecmp(value, "invalid date") == 0)
		return 1;

	if (contains_nan_ci(value))
		return 1;

	return 0;
}

static void trim_copy(const char *in, char *out, size_t out_len)
{
	const char *s;
	const char *e;
	size_t n;

	if (!out || out_len == 0)
		return;

	if (!in)
	{
		out[0] = '\0';
		return;
	}

	s = in;

	while (*s && isspace((unsigned char)*s))
		s++;

	e = s + strlen(s);

	while (e > s && isspace((unsigned char)e[-1]))
		e--;

	n = (size_t)(e - s);

	if (n >= out_len)
		n = out_len - 1;

	memcpy(out, s, n);
	out[n] = '\0';
}

static int is_numeric_string(const char *s)
{
	char *end;

	if (!s || !*s)
		return 0;

	strtod(s, &end);

	while (*end && isspace((unsigned char)*end))
		end++;

	return *end == '\0';
}

static void strip_frac_seconds(char *s)
{
	char *p;

	if (!s)
		return;

	for (p = s; *p; p++)
	{
		if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1]) && p[2] == ':' &&
			isdigit((unsigned char)p[3]) && isdigit((unsigned char)p[4]) && p[5] == ':' &&
			isdigit((unsigned char)p[6]) && isdigit((unsigned char)p[7]) && p[8] == '.')
		{
			const char *rest = p + 9;

			while (*rest >= '0' && *rest <= '9')
				rest++;

			memmove(p + 8, rest, strlen(rest) + 1);
			return;
		}
	}
}

static int parse_query_date_prefix(const char *digits, int *year, int *month, int *day, int *hour, int *minute, int *second)
{
	char sep1, sep2;
	int n;
	int consumed = 0;
	const char *rest;

	if (!digits || strlen(digits) < 8)
		return -1;

	if (!isdigit((unsigned char)digits[0]) || !isdigit((unsigned char)digits[1]) ||
		!isdigit((unsigned char)digits[2]) || !isdigit((unsigned char)digits[3]))
		return -1;

	n = sscanf(digits, "%d%c%d%c%d%n", year, &sep1, month, &sep2, day, &consumed);

	if (n != 5 || (sep1 != '/' && sep1 != '-' && sep1 != '.') || sep1 != sep2)
		return -1;

	*hour = 0;
	*minute = 0;
	*second = 0;
	rest = digits + consumed;

	if (*rest == ' ' || *rest == 'T')
	{
		int h = 0, mi = 0, sc = 0;
		int used = 0;
		rest++;
		n = sscanf(rest, "%d:%d%n", &h, &mi, &used);

		if (n >= 2)
		{
			*hour = h;
			*minute = mi;
			rest = rest + used;

			if (*rest == ':')
			{
				rest++;

				if (isdigit((unsigned char)rest[0]) && isdigit((unsigned char)rest[1]))
				{
					sc = (rest[0] - '0') * 10 + (rest[1] - '0');
					*second = sc;
				}
			}
		}
	}

	return 0;
}

static void year_band_defaults(const jalalix_normalize_options *opts, int *ymin, int *ymax)
{
	*ymin = (!opts || opts->jalali_year_min == 0) ? 1 : opts->jalali_year_min;
	*ymax = (!opts || opts->jalali_year_max == 0) ? 1899 : opts->jalali_year_max;
}

int jalalix_normalize_query_timestamp(int64_t timestamp, const jalalix_normalize_options *opts, char *out, size_t out_len)
{
	jalalix_t *j;
	int64_t ts = timestamp;
	char buf[64];
	int w;

	if (!out || out_len == 0)
		return -1;

	if (ts > 9999999999LL)
		ts /= 1000;

	if (ts <= 0)
	{
		out[0] = '\0';
		return 1;
	}

	j = jalalix_create_ms(ts * 1000);

	if (!j)
		return -1;

	if (opts && opts->timezone)
		(void)jalalix_set_timezone(j, opts->timezone);

	w = jalalix_format(j, "YYYY-MM-DD HH:mm:ss", buf, sizeof(buf), 0, 1);
	jalalix_free(j);

	if (w < 0)
		return -1;

	return write_query_out(out, out_len, buf);
}

int jalalix_normalize_query_date(const char *value, const jalalix_normalize_options *opts, char *out, size_t out_len)
{
	char digits[512];
	char trimmed[512];
	char jalali_in[64];
	char formatted[64];
	int year, month, day, hour, minute, second;
	int jalali_min, jalali_max;
	jalalix_t *j;
	int w;

	if (!out || out_len == 0)
		return -1;

	if (is_query_sentinel(value))
	{
		out[0] = '\0';
		return 1;
	}

	trim_copy(value, trimmed, sizeof(trimmed));

	if (trimmed[0] && is_numeric_string(trimmed))
		return jalalix_normalize_query_timestamp((int64_t)strtod(trimmed, NULL), opts, out, out_len);

	if (jalalix_from_persian(value, digits, sizeof(digits)) != 0)
		return -1;

	strip_frac_seconds(digits);

	if (parse_query_date_prefix(digits, &year, &month, &day, &hour, &minute, &second) != 0)
		return write_query_out(out, out_len, digits);

	year_band_defaults(opts, &jalali_min, &jalali_max);

	if (year >= jalali_min && year <= jalali_max && is_valid_jalali_civil_date(year, month, day))
	{
		snprintf(jalali_in, sizeof(jalali_in), "%04d/%02d/%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
		j = jalalix_parse_jalali(jalali_in);

		if (j)
		{
			if (opts && opts->timezone)
				(void)jalalix_set_timezone(j, opts->timezone);

			w = jalalix_format(j, "YYYY-MM-DD HH:mm:ss", formatted, sizeof(formatted), 0, 1);
			jalalix_free(j);

			if (w < 0)
				return -1;

			return write_query_out(out, out_len, formatted);
		}
	}

	if (is_valid_gregorian_civil_date(year, month, day))
	{
		snprintf(formatted, sizeof(formatted), "%04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);

		return write_query_out(out, out_len, formatted);
	}

	return write_query_out(out, out_len, digits);
}

int jalalix_compare(const jalalix_t *a, const jalalix_t *b)
{
	if (!a || !b)
		return 0;

	return a->ms < b->ms ? 1 : 0;
}

typedef struct
{
	char **items;
	size_t count;
	size_t cap;
} ph_list;

static int ph_push(ph_list *list, const char *val)
{
	char *copy;

	if (list->count >= list->cap)
	{
		size_t ncap = list->cap ? list->cap * 2 : 16;
		char **nitems = (char **)realloc(list->items, ncap * sizeof(char *));

		if (!nitems)
			return -1;

		list->items = nitems;
		list->cap = ncap;
	}

	copy = (char *)malloc(strlen(val) + 1);

	if (!copy)
		return -1;

	strcpy(copy, val);
	list->items[list->count++] = copy;

	return (int)(list->count - 1);
}

static void ph_free(ph_list *list)
{
	size_t i;

	for (i = 0; i < list->count; i++)
		free(list->items[i]);

	free(list->items);
	list->items = NULL;
	list->count = list->cap = 0;
}

static char *str_replace_all(const char *src, const char *from, const char *to)
{
	size_t from_len = strlen(from);
	size_t to_len = strlen(to);
	size_t count = 0;
	const char *p;
	char *out;
	char *w;
	size_t out_len;

	if (from_len == 0)
	{
		out = (char *)malloc(strlen(src) + 1);

		if (out)
			strcpy(out, src);

		return out;
	}

	for (p = src; (p = strstr(p, from)); p += from_len)
		count++;

	out_len = strlen(src) + count * (to_len >= from_len ? to_len - from_len : 0) + count * to_len + 1;

	// safer size
	out_len = strlen(src) + 1 + count * (to_len + 1);
	out = (char *)malloc(out_len);

	if (!out)
		return NULL;

	w = out;
	p = src;

	while (*p)
	{
		if (strncmp(p, from, from_len) == 0)
		{
			memcpy(w, to, to_len);
			w += to_len;
			p += from_len;
		}
		else
		{
			*w++ = *p++;
		}
	}

	*w = '\0';

	return out;
}

static char *replace_token(char *output, const char *token, ph_list *ph, const char *val)
{
	char marker[64];
	char *next;
	int idx;

	if (!strstr(output, token))
		return output;

	idx = ph_push(ph, val);

	if (idx < 0)
		return output;

	snprintf(marker, sizeof(marker), "\xEE\x80\x80%d\xEE\x80\x81", idx);
	next = str_replace_all(output, token, marker);

	if (next)
	{
		free(output);
		return next;
	}

	return output;
}

int jalalix_format(const jalalix_t *j, const char *fmt, char *out, size_t out_len, int persian_digits, int gregorian)
{
	ph_list ph = {0};
	char *output = NULL;
	struct tm tm;
	int ms_part;
	int year, month, date, week_of_year, day_of_year;
	const char *month_name;
	const char *week_day_name;
	const char *week_day_letter;
	const char *meridiem;
	char year_str[16];
	char buf[64];
	int hours, hours_iso;
	size_t i;
	char marker_from[64];
	int n;

	if (!j || !fmt || !out || out_len == 0)
		return -1;

	ensure_jalali_const(j);
	{
		int wy, wh, wmi, ws, wmsp;
		unsigned wm, wd;
		int wday;
		ms_to_civil(wall_ms_of(j), &wy, &wm, &wd, &wh, &wmi, &ws, &wmsp);
		tm.tm_year = wy - 1900;
		tm.tm_mon = (int)wm - 1;
		tm.tm_mday = (int)wd;
		tm.tm_hour = wh;
		tm.tm_min = wmi;
		tm.tm_sec = ws;
		wday = gregorian_weekday_wall(j);
		tm.tm_wday = wday;
		ms_part = wmsp;
		(void)ms_part;
	}

	if (gregorian)
	{
		year = tm.tm_year + 1900;
		month = tm.tm_mon + 1;
		date = tm.tm_mday;
		week_of_year = get_gregorian_week_of_year(j);
		day_of_year = get_gregorian_day_of_year(j);
		month_name = MONTHS_GREG_NAMES[month - 1];
		week_day_name = WEEK_DAYS_GREG_NAMES[tm.tm_wday];
		week_day_letter = WEEK_DAYS_GREG_LETTERS[tm.tm_wday];
		meridiem = tm.tm_hour >= 12 ? PM_GREG : AM_GREG;
	}
	else
	{
		year = j->year;
		month = j->month;
		date = j->day;
		week_of_year = get_week_of_year_jalali(j);
		day_of_year = get_day_of_year_jalali(j);
		month_name = MONTHS_NAMES[month - 1];
		week_day_name = WEEK_DAYS_NAMES[(tm.tm_wday + JX_FIRST_DAY_OF_WEEK) % 7];
		week_day_letter = WEEK_DAYS_LETTERS[(tm.tm_wday + JX_FIRST_DAY_OF_WEEK) % 7];
		meridiem = tm.tm_hour > 12 ? PM : AM;
	}

	snprintf(year_str, sizeof(year_str), "%d", year);
	output = (char *)malloc(strlen(fmt) + 1);

	if (!output)
		return -1;
	strcpy(output, fmt);

	if (strstr(output, "YYYY"))
		output = replace_token(output, "YYYY", &ph, year_str);

	if (strstr(output, "YYY"))
		output = replace_token(output, "YYY", &ph, year_str + (strlen(year_str) > 3 ? strlen(year_str) - 3 : 0));

	if (strstr(output, "YY"))
		output = replace_token(output, "YY", &ph, year_str + (strlen(year_str) > 2 ? strlen(year_str) - 2 : 0));

	if (strstr(output, "Y"))
		output = replace_token(output, "Y", &ph, year_str + (strlen(year_str) > 1 ? strlen(year_str) - 1 : 0));

	if (strstr(output, "MMM"))
		output = replace_token(output, "MMM", &ph, month_name);

	add_zero(buf, sizeof(buf), month);

	if (strstr(output, "MM"))
		output = replace_token(output, "MM", &ph, buf);

	snprintf(buf, sizeof(buf), "%d", month);

	if (strstr(output, "M"))
		output = replace_token(output, "M", &ph, buf);

	add_zero(buf, sizeof(buf), week_of_year);

	if (strstr(output, "W"))
		output = replace_token(output, "W", &ph, buf);

	snprintf(buf, sizeof(buf), "%d", week_of_year);

	if (strstr(output, "w"))
		output = replace_token(output, "w", &ph, buf);

	snprintf(buf, sizeof(buf), "%d", get_day_iso(j));

	if (strstr(output, "DW"))
		output = replace_token(output, "DW", &ph, buf);
	{
		int dw = gregorian ? (tm.tm_wday == 0 ? 7 : tm.tm_wday) : get_day_jalali(j);
		snprintf(buf, sizeof(buf), "%d", dw);
		if (strstr(output, "dw"))
			output = replace_token(output, "dw", &ph, buf);
	}

	if (strstr(output, "WN"))
		output = replace_token(output, "WN", &ph, week_day_name);

	if (strstr(output, "wn"))
		output = replace_token(output, "wn", &ph, week_day_name);

	if (strstr(output, "WL"))
		output = replace_token(output, "WL", &ph, week_day_letter);

	if (strstr(output, "wl"))
		output = replace_token(output, "wl", &ph, week_day_letter);

	add_zero(buf, sizeof(buf), day_of_year);

	if (strstr(output, "DDDD"))
		output = replace_token(output, "DDDD", &ph, buf);

	snprintf(buf, sizeof(buf), "%d", day_of_year);

	if (strstr(output, "DDD"))
		output = replace_token(output, "DDD", &ph, buf);

	add_zero(buf, sizeof(buf), date);

	if (strstr(output, "DD"))
		output = replace_token(output, "DD", &ph, buf);

	snprintf(buf, sizeof(buf), "%d", date);

	if (strstr(output, "D"))
		output = replace_token(output, "D", &ph, buf);

	hours = tm.tm_hour;
	add_zero(buf, sizeof(buf), hours);

	if (strstr(output, "HH"))
		output = replace_token(output, "HH", &ph, buf);

	snprintf(buf, sizeof(buf), "%d", hours);

	if (strstr(output, "H"))
		output = replace_token(output, "H", &ph, buf);

	hours_iso = hours > 12 ? hours - 12 : (hours == 0 ? 12 : hours);
	add_zero(buf, sizeof(buf), hours_iso);

	if (strstr(output, "hh"))
		output = replace_token(output, "hh", &ph, buf);

	snprintf(buf, sizeof(buf), "%d", hours_iso);

	if (strstr(output, "h"))
		output = replace_token(output, "h", &ph, buf);

	add_zero(buf, sizeof(buf), tm.tm_min);

	if (strstr(output, "mm"))
		output = replace_token(output, "mm", &ph, buf);

	snprintf(buf, sizeof(buf), "%d", tm.tm_min);

	if (strstr(output, "m"))
		output = replace_token(output, "m", &ph, buf);

	add_zero(buf, sizeof(buf), tm.tm_sec);

	if (strstr(output, "ss"))
		output = replace_token(output, "ss", &ph, buf);

	snprintf(buf, sizeof(buf), "%d", tm.tm_sec);

	if (strstr(output, "s"))
		output = replace_token(output, "s", &ph, buf);

	snprintf(buf, sizeof(buf), "%d", jalalix_get_milliseconds(j));

	if (strstr(output, "sss"))
		output = replace_token(output, "sss", &ph, buf);

	if (strstr(output, "SSS"))
		output = replace_token(output, "SSS", &ph, buf);

	if (strstr(output, "A"))
		output = replace_token(output, "A", &ph, meridiem);

	if (strstr(output, "a"))
		output = replace_token(output, "a", &ph, meridiem);

	snprintf(buf, sizeof(buf), "%lld", (long long)(j->ms / 1000));

	if (strstr(output, "X"))
		output = replace_token(output, "X", &ph, buf);

	snprintf(buf, sizeof(buf), "%lld", (long long)j->ms);

	if (strstr(output, "x"))
		output = replace_token(output, "x", &ph, buf);

	for (i = 0; i < ph.count; i++)
	{
		char *next;
		snprintf(marker_from, sizeof(marker_from), "\xEE\x80\x80%zu\xEE\x80\x81", i);
		next = str_replace_all(output, marker_from, ph.items[i]);

		if (next)
		{
			free(output);
			output = next;
		}
	}

	if (persian_digits)
	{
		char persian[1024];

		if (jalalix_to_persian(output, persian, sizeof(persian)) == 0)
		{
			free(output);
			output = (char *)malloc(strlen(persian) + 1);

			if (!output)
			{
				ph_free(&ph);
				return -1;
			}

			strcpy(output, persian);
		}
	}

	if (strlen(output) + 1 > out_len)
	{
		free(output);
		ph_free(&ph);

		return -1;
	}

	strcpy(out, output);
	n = (int)strlen(out);
	free(output);
	ph_free(&ph);

	return n;
}
