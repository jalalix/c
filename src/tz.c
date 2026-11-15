/**
 * Pure TZif v2/v3 reader + IANA resolution. No external libraries.
 */
#include "tz.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
static int jx_strcasecmp(const char *a, const char *b)
{
	while (*a && *b)
	{
		int ca = tolower((unsigned char)*a++);
		int cb = tolower((unsigned char)*b++);
		if (ca != cb)
			return ca - cb;
	}
	return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}
#define strcasecmp jx_strcasecmp
#else
#include <strings.h>
#endif

#ifdef _WIN32
// No /usr/share/zoneinfo on Windows - embedded only.
#else
#include <unistd.h>
#endif

#define JX_TZ_MAX_TRANS 1024
#define JX_TZ_MAX_TYPES 256
#define JX_TZ_ID_MAX 128

typedef struct
{
	int32_t utoff;
} jx_ttype;

typedef struct
{
	int64_t transitions[JX_TZ_MAX_TRANS];
	unsigned char trans_types[JX_TZ_MAX_TRANS];
	jx_ttype types[JX_TZ_MAX_TYPES];
	int timecnt;
	int typecnt;
	char posix[128];
} jx_tzdb;

static int read_i32_be(const unsigned char *p, size_t len, size_t *off, int32_t *out)
{
	if (*off + 4 > len)
		return -1;

	*out = (int32_t)(((uint32_t)p[*off] << 24) | ((uint32_t)p[*off + 1] << 16) | ((uint32_t)p[*off + 2] << 8) | (uint32_t)p[*off + 3]);
	*off += 4;

	return 0;
}

static int read_i64_be(const unsigned char *p, size_t len, size_t *off, int64_t *out)
{
	int i;
	uint64_t v = 0;

	if (*off + 8 > len)
		return -1;

	for (i = 0; i < 8; i++)
		v = (v << 8) | p[*off + i];

	*out = (int64_t)v;
	*off += 8;

	return 0;
}

static int is_leap(int y)
{
	return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int64_t days_from_civil(int y, int m, int d)
{
	y -= m <= 2;

	{
		const int64_t era = (y >= 0 ? y : y - 399) / 400;
		const unsigned yoe = (unsigned)(y - era * 400);
		const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
		const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		return era * 146097 + (int64_t)doe - 719468;
	}
}

static int civil_year_from_unix(int64_t unix_sec)
{
	int64_t days = unix_sec >= 0 ? unix_sec / 86400 : (unix_sec - 86399) / 86400;
	int64_t z = days + 719468;
	int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	unsigned doe = (unsigned)(z - era * 146097);
	unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int64_t y = (int64_t)yoe + era * 400;
	unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	unsigned mp = (5 * doy + 2) / 153;
	unsigned m = mp < 10 ? mp + 3 : mp - 9;
	y += (m <= 2);
	return (int)y;
}

// Parse POSIX ±H[:M[:S]] as written; return seconds east of UTC (negate west-positive).
static int parse_posix_offset(const char *s, int32_t *east_out, const char **rest_out)
{
	int sign = 1;
	int hours = 0, minutes = 0, seconds = 0;
	int west;
	const char *p = s;

	if (!p || !*p)
		return -1;

	if (*p == '+')
	{
		sign = 1;
		p++;
	}
	else if (*p == '-')
	{
		sign = -1;
		p++;
	}

	if (!isdigit((unsigned char)*p))
		return -1;

	hours = 0;

	while (isdigit((unsigned char)*p))
		hours = hours * 10 + (*p++ - '0');

	if (*p == ':')
	{
		p++;

		while (isdigit((unsigned char)*p))
			minutes = minutes * 10 + (*p++ - '0');

		if (*p == ':')
		{
			p++;

			while (isdigit((unsigned char)*p))
				seconds = seconds * 10 + (*p++ - '0');
		}
	}

	west = sign * (hours * 3600 + minutes * 60 + seconds);
	*east_out = -west;

	if (rest_out)
		*rest_out = p;

	return 0;
}

static const char *skip_posix_abbr(const char *s)
{
	if (!s)
		return NULL;

	if (*s == '<')
	{
		const char *e = strchr(s, '>');
		return e ? e + 1 : NULL;
	}

	while (*s && isalpha((unsigned char)*s))
		s++;

	return s;
}

static int parse_hhmmss(const char *s)
{
	int h = 0, m = 0, sec = 0;

	if (!s || !*s)
		return 2 * 3600;

	sscanf(s, "%d:%d:%d", &h, &m, &sec);

	return h * 3600 + m * 60 + sec;
}

static int parse_m_rule_doy(int year, const char *rest)
{
	int month, n, d, dim, first, day, day1_wday;
	int64_t day1;
	int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	if (sscanf(rest, "%d.%d.%d", &month, &n, &d) != 3)
		return -1;

	if (month < 1 || month > 12 || n < 1 || n > 5 || d < 0 || d > 6)
		return -1;

	days_in_month[1] += is_leap(year);
	dim = days_in_month[month - 1];
	day1 = days_from_civil(year, month, 1);
	day1_wday = (int)((day1 + 4) % 7);

	if (day1_wday < 0)
		day1_wday += 7;

	first = 1 + ((d - day1_wday) % 7 + 7) % 7;

	if (n == 5)
	{
		day = first;

		while (day + 7 <= dim)
			day += 7;
	}
	else
	{
		day = first + (n - 1) * 7;

		if (day > dim)
			return -1;
	}

	return (int)(days_from_civil(year, month, day) - days_from_civil(year, 1, 1));
}

static int64_t rule_to_unix(int year, const char *rule, int32_t offset_before)
{
	char date_part[64];
	const char *slash;
	int time_secs;
	int doy;
	int64_t local_secs;

	slash = strchr(rule, '/');

	if (slash)
	{
		size_t n = (size_t)(slash - rule);

		if (n >= sizeof(date_part))
			n = sizeof(date_part) - 1;

		memcpy(date_part, rule, n);
		date_part[n] = '\0';
		time_secs = parse_hhmmss(slash + 1);
	}
	else
	{
		strncpy(date_part, rule, sizeof(date_part) - 1);
		date_part[sizeof(date_part) - 1] = '\0';
		time_secs = 2 * 3600;
	}

	if (date_part[0] == 'J')
	{
		int n = atoi(date_part + 1);

		if (n < 1 || n > 365)
			return 0;
		if (is_leap(year) && n >= 60)
			doy = n;
		else
			doy = n - 1;
	}
	else if (date_part[0] == 'M')
	{
		doy = parse_m_rule_doy(year, date_part + 1);
		if (doy < 0)
			return 0;
	}
	else
	{
		doy = atoi(date_part);
	}

	local_secs = days_from_civil(year, 1, 1) * 86400 + (int64_t)doy * 86400 + time_secs;

	return local_secs - (int64_t)offset_before;
}

static int posix_offset_at(const char *tz, int64_t unix_sec, int32_t *out)
{
	const char *after_abbr;
	const char *after_off;
	int32_t std_off, dst_off;
	char std_buf[128];
	const char *rest;
	size_t std_len;

	if (!tz || !*tz)
		return -1;

	after_abbr = skip_posix_abbr(tz);

	if (!after_abbr || parse_posix_offset(after_abbr, &std_off, &after_off) != 0)
		return -1;

	std_len = (size_t)(after_off - tz);

	if (std_len >= sizeof(std_buf))
		std_len = sizeof(std_buf) - 1;

	memcpy(std_buf, tz, std_len);
	std_buf[std_len] = '\0';
	(void)std_buf;

	rest = after_off;

	if (!rest || !*rest)
	{
		*out = std_off;
		return 0;
	}

	after_abbr = skip_posix_abbr(rest);

	if (!after_abbr)
		return -1;

	if (!*after_abbr || *after_abbr == ',')
	{
		dst_off = std_off + 3600;
		rest = after_abbr;
	}
	else if (*after_abbr == '+' || *after_abbr == '-' || isdigit((unsigned char)*after_abbr))
	{
		if (parse_posix_offset(after_abbr, &dst_off, &rest) != 0)
			return -1;
	}
	else
	{
		dst_off = std_off + 3600;
		rest = after_abbr;
	}

	if (!rest || *rest != ',')
	{
		*out = std_off;
		return 0;
	}
	{
		char rules[128];
		char *comma;
		char *start_rule;
		char *end_rule;
		int year;
		int64_t start_utc, end_utc;
		int in_dst;

		strncpy(rules, rest + 1, sizeof(rules) - 1);
		rules[sizeof(rules) - 1] = '\0';
		comma = strchr(rules, ',');

		if (!comma)
			return -1;

		*comma = '\0';
		start_rule = rules;
		end_rule = comma + 1;
		year = civil_year_from_unix(unix_sec);
		start_utc = rule_to_unix(year, start_rule, std_off);
		end_utc = rule_to_unix(year, end_rule, dst_off);

		if (start_utc < end_utc)
			in_dst = unix_sec >= start_utc && unix_sec < end_utc;
		else
			in_dst = unix_sec >= start_utc || unix_sec < end_utc;

		*out = in_dst ? dst_off : std_off;

		return 0;
	}
}

static int parse_tzif(const unsigned char *data, size_t len, jx_tzdb *db)
{
	size_t off;
	int32_t ttisutcnt, ttisstdcnt, leapcnt, timecnt, typecnt, charcnt;
	int32_t ttisutcnt2, ttisstdcnt2, leapcnt2, timecnt2, typecnt2, charcnt2;
	int i;
	unsigned char version;

	memset(db, 0, sizeof(*db));

	if (len < 44 || memcmp(data, "TZif", 4) != 0)
		return -1;

	version = data[4];

	off = 20;

	if (read_i32_be(data, len, &off, &ttisutcnt) || read_i32_be(data, len, &off, &ttisstdcnt) ||
		read_i32_be(data, len, &off, &leapcnt) || read_i32_be(data, len, &off, &timecnt) ||
		read_i32_be(data, len, &off, &typecnt) || read_i32_be(data, len, &off, &charcnt))
		return -1;

	off = 44;
	off += (size_t)timecnt * 4;
	off += (size_t)timecnt;
	off += (size_t)typecnt * 6;
	off += (size_t)charcnt;
	off += (size_t)leapcnt * 8;
	off += (size_t)ttisstdcnt;
	off += (size_t)ttisutcnt;

	if (version == 0)
	{
		// v1 only - re-read 32-bit body
		off = 44;

		if (timecnt > JX_TZ_MAX_TRANS || typecnt > JX_TZ_MAX_TYPES || typecnt < 1)
			return -1;

		db->timecnt = timecnt;
		db->typecnt = typecnt;

		for (i = 0; i < timecnt; i++)
		{
			int32_t t;

			if (read_i32_be(data, len, &off, &t))
				return -1;

			db->transitions[i] = t;
		}

		for (i = 0; i < timecnt; i++)
		{
			if (off >= len)
				return -1;

			db->trans_types[i] = data[off++];
		}

		for (i = 0; i < typecnt; i++)
		{
			int32_t utoff;

			if (read_i32_be(data, len, &off, &utoff))
				return -1;

			if (off + 2 > len)
				return -1;

			off += 2; // isdst + abbrind
			db->types[i].utoff = utoff;
		}

		return 0;
	}

	if (off + 44 > len || memcmp(data + off, "TZif", 4) != 0)
		return -1;

	off += 20;

	if (read_i32_be(data, len, &off, &ttisutcnt2) || read_i32_be(data, len, &off, &ttisstdcnt2) ||
		read_i32_be(data, len, &off, &leapcnt2) || read_i32_be(data, len, &off, &timecnt2) ||
		read_i32_be(data, len, &off, &typecnt2) || read_i32_be(data, len, &off, &charcnt2))
		return -1;

	if (timecnt2 > JX_TZ_MAX_TRANS || typecnt2 > JX_TZ_MAX_TYPES || typecnt2 < 1)
		return -1;

	db->timecnt = timecnt2;
	db->typecnt = typecnt2;

	for (i = 0; i < timecnt2; i++)
	{
		if (read_i64_be(data, len, &off, &db->transitions[i]))
			return -1;
	}

	for (i = 0; i < timecnt2; i++)
	{
		if (off >= len)
			return -1;
		db->trans_types[i] = data[off++];
	}

	for (i = 0; i < typecnt2; i++)
	{
		int32_t utoff;

		if (read_i32_be(data, len, &off, &utoff))
			return -1;

		if (off + 2 > len)
			return -1;

		off += 2;
		db->types[i].utoff = utoff;
	}

	off += (size_t)charcnt2;
	off += (size_t)leapcnt2 * 12;
	off += (size_t)ttisstdcnt2;
	off += (size_t)ttisutcnt2;

	if (off < len)
	{
		const unsigned char *rest = data + off;
		size_t rem = len - off;
		size_t a, b;

		for (a = 0; a < rem && rest[a] != '\n'; a++)
			;

		if (a < rem)
		{
			a++;

			for (b = a; b < rem && rest[b] != '\n'; b++)
				;

			if (b > a)
			{
				size_t n = b - a;

				if (n >= sizeof(db->posix))
					n = sizeof(db->posix) - 1;

				memcpy(db->posix, rest + a, n);
				db->posix[n] = '\0';
			}
		}
	}

	return 0;
}

static int32_t db_offset_at(const jx_tzdb *db, int64_t unix_sec)
{
	int lo, hi, mid, idx;
	int ti;
	int32_t posix_off;

	if (db->typecnt < 1)
		return 0;

	if (db->timecnt == 0)
		return db->types[0].utoff;

	if (unix_sec < db->transitions[0])
		return db->types[0].utoff;

	lo = 0;
	hi = db->timecnt;

	while (lo < hi)
	{
		mid = (lo + hi) / 2;

		if (db->transitions[mid] <= unix_sec)
			lo = mid + 1;
		else
			hi = mid;
	}

	idx = lo - 1;

	if (idx + 1 == db->timecnt && db->posix[0] &&
		posix_offset_at(db->posix, unix_sec, &posix_off) == 0)
		return posix_off;

	ti = db->trans_types[idx];

	if (ti < 0 || ti >= db->typecnt)
		return db->types[0].utoff;

	return db->types[ti].utoff;
}

static int load_bytes(const char *iana_id, unsigned char **out_buf, size_t *out_len, int *owned)
{
	char path[512];
	FILE *f;
	long sz;
	unsigned char *buf;
	const jx_tz_embed_t *e;

	*owned = 0;
	*out_buf = NULL;
	*out_len = 0;

#ifndef _WIN32
	snprintf(path, sizeof(path), "/usr/share/zoneinfo/%s", iana_id);
	f = fopen(path, "rb");

	if (!f)
	{
		snprintf(path, sizeof(path), "/var/db/timezone/zoneinfo/%s", iana_id);
		f = fopen(path, "rb");
	}

	if (f)
	{
		if (fseek(f, 0, SEEK_END) == 0)
		{
			sz = ftell(f);

			if (sz > 0 && fseek(f, 0, SEEK_SET) == 0)
			{
				buf = (unsigned char *)malloc((size_t)sz);

				if (buf && fread(buf, 1, (size_t)sz, f) == (size_t)sz && sz >= 4 &&
					memcmp(buf, "TZif", 4) == 0)
				{
					fclose(f);
					*out_buf = buf;
					*out_len = (size_t)sz;
					*owned = 1;
					return 0;
				}

				free(buf);
			}
		}

		fclose(f);
	}
#endif

	for (e = jx_tz_embedded; e->id; e++)
	{
		if (strcmp(e->id, iana_id) == 0)
		{
			*out_buf = (unsigned char *)(uintptr_t)e->data;
			*out_len = e->len;
			*owned = 0;
			return 0;
		}
	}

	for (e = jx_tz_embedded; e->id; e++)
	{
		if (strcasecmp(e->id, iana_id) == 0)
		{
			*out_buf = (unsigned char *)(uintptr_t)e->data;
			*out_len = e->len;
			*owned = 0;
			return 0;
		}
	}

	return -1;
}

static void canonicalize_id(const char *id, char *out, size_t out_len)
{
	const jx_tz_embed_t *e;
	char lower[JX_TZ_ID_MAX];
	size_t i, n;

	n = strlen(id);

	if (n >= sizeof(lower))
		n = sizeof(lower) - 1;

	for (i = 0; i < n; i++)
		lower[i] = (char)tolower((unsigned char)id[i]);

	lower[n] = '\0';

	if (strcmp(lower, "utc") == 0 || strcmp(lower, "gmt") == 0)
	{
		snprintf(out, out_len, "UTC");
		return;
	}

	if (strcmp(lower, "etc/utc") == 0)
	{
		snprintf(out, out_len, "Etc/UTC");
		return;
	}

	for (e = jx_tz_embedded; e->id; e++)
	{
		if (strcasecmp(e->id, id) == 0)
		{
			snprintf(out, out_len, "%s", e->id);
			return;
		}
	}

	// Title-case segments
	{
		char tmp[JX_TZ_ID_MAX];
		size_t j = 0;
		int cap = 1;

		for (i = 0; id[i] && j + 1 < sizeof(tmp); i++)
		{
			char c = id[i];

			if (c == '/')
			{
				tmp[j++] = c;
				cap = 1;
			}
			else if (cap && isalpha((unsigned char)c))
			{
				tmp[j++] = (char)toupper((unsigned char)c);
				cap = 0;
			}
			else
			{
				tmp[j++] = c;
				if (c == '_')
					cap = 1;
			}
		}

		tmp[j] = '\0';
		snprintf(out, out_len, "%s", tmp);
	}
}

static int zone_loadable(const char *id)
{
	unsigned char *buf;
	size_t len;
	int owned;

	if (load_bytes(id, &buf, &len, &owned) != 0)
		return 0;
	if (owned)
		free(buf);

	return 1;
}

static char g_host_tz[JX_TZ_ID_MAX] = "UTC";
static int g_host_tz_ready = 0;

const char *jx_tz_detect_host(void)
{
	const char *tz;
	char link[512];

	if (g_host_tz_ready)
		return g_host_tz;

	tz = getenv("TZ");

	if (tz && tz[0])
	{
		if (tz[0] == ':')
		{
			const char *p = strstr(tz + 1, "zoneinfo/");
			if (p)
			{
				p += 9;

				if (*p == '/')
					p++;

				canonicalize_id(p, g_host_tz, sizeof(g_host_tz));
				g_host_tz_ready = 1;

				return g_host_tz;
			}
		}
		else if (strchr(tz, '/') && !strchr(tz, '<'))
		{
			canonicalize_id(tz, g_host_tz, sizeof(g_host_tz));

			if (zone_loadable(g_host_tz))
			{
				g_host_tz_ready = 1;
				return g_host_tz;
			}
		}
	}

#ifndef _WIN32
	{
		ssize_t n = readlink("/etc/localtime", link, sizeof(link) - 1);

		if (n > 0)
		{
			const char *p;
			link[n] = '\0';
			p = strstr(link, "zoneinfo/");

			if (p)
			{
				p += 9;

				if (*p == '/')
					p++;

				canonicalize_id(p, g_host_tz, sizeof(g_host_tz));
				g_host_tz_ready = 1;

				return g_host_tz;
			}
		}
	}
#endif

	snprintf(g_host_tz, sizeof(g_host_tz), "UTC");
	g_host_tz_ready = 1;

	return g_host_tz;
}

int jx_tz_resolve(const char *input, const char *current, char *out, size_t out_len)
{
	char canon[JX_TZ_ID_MAX];
	char lower[JX_TZ_ID_MAX];
	size_t i, n;

	if (!out || out_len == 0 || !current)
		return -1;

	if (!input)
	{
		snprintf(out, out_len, "%s", current);
		return 0;
	}

	while (*input && isspace((unsigned char)*input))
		input++;

	if (!*input)
		return -1;

	n = strlen(input);

	if (n >= sizeof(lower))
		n = sizeof(lower) - 1;

	for (i = 0; i < n; i++)
		lower[i] = (char)tolower((unsigned char)input[i]);

	lower[n] = '\0';

	if (strcmp(lower, "default") == 0 || strcmp(lower, "local") == 0 ||
		strcmp(lower, "system") == 0)
	{
		snprintf(out, out_len, "%s", jx_tz_detect_host());
		return 0;
	}

	if (strcmp(lower, "utc") == 0 || strcmp(lower, "gmt") == 0)
	{
		snprintf(out, out_len, "UTC");
		return 0;
	}

	canonicalize_id(input, canon, sizeof(canon));

	if (strcmp(canon, "UTC") == 0 || zone_loadable(canon) || zone_loadable(input))
	{
		snprintf(out, out_len, "%s", canon);
		return 0;
	}

	return -1;
}

int jx_tz_offset_seconds(const char *iana_id, int64_t unix_ms, int32_t *out_offset)
{
	unsigned char *buf;
	size_t len;
	int owned;
	jx_tzdb db;
	int64_t unix_sec;

	if (!iana_id || !out_offset)
		return -1;

	if (strcmp(iana_id, "UTC") == 0 || strcasecmp(iana_id, "gmt") == 0)
	{
		*out_offset = 0;
		return 0;
	}

	if (load_bytes(iana_id, &buf, &len, &owned) != 0)
		return -1;

	if (parse_tzif(buf, len, &db) != 0)
	{
		if (owned)
			free(buf);
		return -1;
	}

	if (owned)
		free(buf);

	unix_sec = unix_ms >= 0 ? unix_ms / 1000 : (unix_ms - 999) / 1000;

	// floor toward -inf for negative
	if (unix_ms < 0)
	{
		unix_sec = unix_ms / 1000;

		if (unix_ms % 1000 != 0)
			unix_sec -= 1;
	}

	*out_offset = db_offset_at(&db, unix_sec);

	return 0;
}

int jx_tz_utc_to_wall_ms(const char *iana_id, int64_t unix_ms, int64_t *out_wall)
{
	int32_t off;

	if (!out_wall)
		return -1;

	if (jx_tz_offset_seconds(iana_id, unix_ms, &off) != 0)
		return -1;

	*out_wall = unix_ms + (int64_t)off * 1000;

	return 0;
}

int jx_tz_wall_to_utc_ms(const char *iana_id, int y, int m, int d, int h, int mi, int s, int ms,
						 int64_t *out_utc)
{
	int64_t wall_as_utc;
	int64_t utc;
	int i;
	int32_t off;

	if (!out_utc)
		return -1;

	wall_as_utc = days_from_civil(y, m, d) * 86400000LL + (int64_t)(h * 3600 + mi * 60 + s) * 1000 + ms;

	if (!iana_id || strcmp(iana_id, "UTC") == 0)
	{
		*out_utc = wall_as_utc;
		return 0;
	}

	utc = wall_as_utc;

	for (i = 0; i < 4; i++)
	{
		int64_t next;

		if (jx_tz_offset_seconds(iana_id, utc, &off) != 0)
			return -1;

		next = wall_as_utc - (int64_t)off * 1000;

		if (next == utc)
		{
			*out_utc = utc;
			return 0;
		}

		utc = next;
	}

	*out_utc = utc;

	return 0;
}
