# JalaliX - C

Jalali (Persian / Shamsi) calendar library in pure C99. Same algorithm as [`@jalalix/typescript`](https://github.com/jalalix/typescript): base date **1348/10/11** ↔ **1970-01-01**, leap division `0.24219858156`.

[![license](https://img.shields.io/badge/license-MIT-blue.svg)](./LICENSE)

```c
#include "jalalix.h"

jalalix_t *date = jalalix_create_iso("2025-02-25");
char buf[64];

jalalix_format(date, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
// 1403/12/07

jalalix_t *parsed = jalalix_parse_jalali("1403/12/07");
jalalix_to_iso_date(parsed, buf, sizeof(buf), 0);
// 2025-02-25

jalalix_free(date);
jalalix_free(parsed);
```

`jalalix_to_iso_date` uses wall-clock getters in the active timezone (not a UTC shift).

## Install / build

```bash
# Makefile
make
make test

# or CMake
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Link with `-lm` (math). TZif data is **embedded** (`src/tzdata_embed.c`); OS `/usr/share/zoneinfo` is tried first when present.

Add `include/jalalix.h` and link `libjalalix.a` (or your CMake target).

## Usage

### create / format

```c
jalalix_t *now = jalalix_create_now();
char out[128];

jalalix_format(now, "YYYY/MM/DD", out, sizeof(out), 0, 0);
jalalix_format(now, "WN، DD MMM YYYY", out, sizeof(out), 0, 0);
jalalix_format(now, "YYYY/MM/DD", out, sizeof(out), 1, 0);   /* Persian digits */
jalalix_format(now, "DD MMM YYYY", out, sizeof(out), 0, 1);  /* Gregorian calendar */

jalalix_free(now);
```

### parseJalali

```c
jalalix_t *d = jalalix_parse_jalali("1403/12/07");
jalalix_t *d2 = jalalix_parse_jalali("۱۴۰۳-۱۲-۰۷ 14:30");
jalalix_t *d3 = jalalix_parse_jalali("1403.12.07");
```

Named `jalalix_parse_jalali` to avoid colliding with other parsers. Accepts `/`, `-`, or `.` as the date separator.

### normalizeQueryDate

Soft-normalize loose API/query date values to a Gregorian `YYYY-MM-DD HH:mm:ss` string (or empty for sentinels). Does not abort on junk input.

```c
char norm[32];
jalalix_normalize_options opts = { .timezone = "Asia/Tehran", .jalali_year_min = 0, .jalali_year_max = 0 };

jalalix_normalize_query_date("1403/12/07", &opts, norm, sizeof(norm));
// norm -> "2025-02-25 00:00:00" (return 0)

jalalix_normalize_query_date("undefined", &opts, norm, sizeof(norm));
// return 1, norm empty

jalalix_normalize_query_timestamp(1740430800000LL, &opts, norm, sizeof(norm));
// "2025-02-25 00:30:00"
```

Handles sentinels (`NULL`, `''`, `'null'`, `'undefined'`, `'Invalid Date'`, `NaN`) → return `1` (empty `out`); unix timestamps (seconds or milliseconds; non-positive → empty); Persian/Arabic-Indic digits; `/` `-` `.` separators; fractional seconds; optional `Z` / offset suffixes.

Detects Jalali when the year is in `jalaliYearMin`…`jalaliYearMax` (default `1`…`1899` when options fields are `0`) **and** the civil date is valid; otherwise Gregorian when valid, or returns the digit-cleaned string.

### toISODate

```c
jalalix_t *d = jalalix_create_iso("2025-02-25T00:30:00+03:30");
char iso[16];

jalalix_to_iso_date(d, iso, sizeof(iso), 0); /* Gregorian -> 2025-02-25 */
jalalix_to_iso_date(d, iso, sizeof(iso), 1); /* Jalali    -> 1403-12-07 */
```

### add / start / end

All of these **mutate** the instance in place (unlike TypeScript, which returns new instances for start/end).

```c
jalalix_t *d = jalalix_create_iso("2025-02-25");
jalalix_add_months(d, 1);
jalalix_start_of_month(d);
/* format -> e.g. 1404/01/01 */

jalalix_set_timezone(d, "Asia/Tehran");
jalalix_add_days(d, 7);
jalalix_end_of_week(d);
```

### Persian / Arabic digits

```c
char persian[64], latin[64];

jalalix_to_persian("1403/12/07", persian, sizeof(persian));     // ۱۴۰۳/۱۲/۰۷
jalalix_from_persian("۱۴۰۳/۱۲/۰۷", latin, sizeof(latin));     // 1403/12/07
jalalix_from_persian("١٤٠٣/١٢/٠٧", latin, sizeof(latin));     // 1403/12/07 (Arabic-Indic)
```

## Timezone

Internal storage is always a **UTC Unix millisecond instant** plus an IANA timezone id. `jalalix_set_timezone` is **instant-preserving**: the timestamp stays the same; only the zone (and therefore wall clock / Jalali day) changes.

```c
jalalix_t *d = jalalix_create_ms(1740430800000LL);
char out[64];

jalalix_set_timezone(d, "Asia/Tehran");
jalalix_format(d, "YYYY/MM/DD", out, sizeof(out), 0, 0); // 1403/12/07
jalalix_format(d, "HH:mm:ss", out, sizeof(out), 0, 0);    // 00:30:00

jalalix_set_timezone(d, "UTC");
jalalix_format(d, "YYYY/MM/DD", out, sizeof(out), 0, 0); // 1403/12/06
jalalix_format(d, "HH:mm:ss", out, sizeof(out), 0, 0);    // 21:00:00

jalalix_get_time(d) /* same before and after zone change */
```

- **Aliases:** `utc` / `gmt` → `UTC`; `local` / `default` / `system` → host zone when detectable, else `UTC`.
- **Invalid zone** → `-1` from `jalalix_set_timezone`.
- Vendored zones: `UTC`, `Etc/UTC`, `Asia/Tehran`, `Asia/Tokyo`, `Europe/London`, `America/New_York`.

## Months

Easy to get wrong:

- `jalalix_get_month()` → **1…12** (Farvardin = 1)
- `jalalix_set_month(m, …)` / `jalalix_set_full_year(y, m, …)` → **0…11**, same as `Date` (Farvardin = 0)

```c
jalalix_t *d = jalalix_create_now();
jalalix_set_full_year(d, 1404, 0, 1); // 1 Farvardin 1404
jalalix_get_month(d); // 1
```

## Format tokens

| Token               | Meaning                         |
| ------------------- | ------------------------------- |
| `YYYY` `YY`         | year                            |
| `MMM` `MM` `M`      | month name / number             |
| `DD` `D`            | day                             |
| `HH` `hh` `mm` `ss` | time                            |
| `A` `a`             | meridiem (ق.ظ. / ب.ظ. or AM/PM) |
| `WN` `WL`           | weekday name / letter           |
| `w` `DDD`           | week of year / day of year      |
| `X` `x`             | unix seconds / ms               |

## API

Opaque handle: `typedef struct jalalix jalalix_t;`

### Types

| API                         | Description                                                                                                                                      |
| --------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `jalalix_t`                 | Opaque Jalali date/time instance (Jalali calendar view over an epoch timestamp).                                                                 |
| `jalalix_normalize_options` | Options for normalize helpers: `timezone` (IANA or `NULL` → default `UTC`), `jalali_year_min` / `jalali_year_max` (`0` → defaults `1` / `1899`). |

### Constructors / lifecycle

| API                           | Description                                                                          |
| ----------------------------- | ------------------------------------------------------------------------------------ |
| `jalalix_create_now()`        | Build from the current local instant (default timezone `UTC`).                       |
| `jalalix_create_ms(ms)`       | Build from Unix milliseconds since 1970-01-01 UTC.                                   |
| `jalalix_create_iso(iso)`     | Build from Gregorian `YYYY-MM-DD` or ISO date/time with optional `Z`.                |
| `jalalix_parse_jalali(value)` | Parse a Jalali date/time string (`/` `-` `.`, Persian/Arabic digits, optional time). |
| `jalalix_clone(j)`            | Copy with the same instant and timezone.                                             |
| `jalalix_free(j)`             | Release memory; `NULL` is a no-op.                                                   |

### Static helpers

| API                                                                | Description                                                                                                                          |
| ------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------ |
| `jalalix_normalize_query_date(value, opts, out, out_len)`          | Soft-normalize loose query/form strings → Gregorian `YYYY-MM-DD HH:mm:ss` in `out`; return `0` ok, `1` sentinel (empty), `-1` error. |
| `jalalix_normalize_query_timestamp(timestamp, opts, out, out_len)` | Same for numeric unix seconds or ms (`> 9999999999` treated as ms); non-positive → empty (`1`).                                      |
| `jalalix_to_persian(in, out, out_len)`                             | Replace Western digits `0-9` with Persian `۰-۹`.                                                                                     |
| `jalalix_from_persian(in, out, out_len)`                           | Replace Persian `۰-۹` and Arabic-Indic `٠-٩` with Western digits.                                                                    |
| `jalalix_compare(a, b)`                                            | `1` if `a` is strictly before `b` (by unix ms), else `0`.                                                                            |

### Instance - getters

| API                           | Description                                                           |
| ----------------------------- | --------------------------------------------------------------------- |
| `jalalix_get_full_year(j)`    | Jalali year in the active timezone.                                   |
| `jalalix_get_month(j)`        | Jalali month **1…12** (Farvardin = 1).                                |
| `jalalix_get_date(j)`         | Jalali day of month **1…31**.                                         |
| `jalalix_get_day(j)`          | Weekday **1…7** (Saturday = 1 … Friday = 7).                          |
| `jalalix_get_hours(j)`        | Wall-clock hours **0…23**.                                            |
| `jalalix_get_minutes(j)`      | Wall-clock minutes **0…59**.                                          |
| `jalalix_get_seconds(j)`      | Wall-clock seconds **0…59**.                                          |
| `jalalix_get_milliseconds(j)` | Wall-clock milliseconds **0…999**.                                    |
| `jalalix_get_time(j)`         | Unix timestamp in milliseconds (UTC instant).                         |
| `jalalix_get_timezone(j)`     | Active IANA timezone id (internal string; valid until next set/free). |

### Instance - setters

| API                                           | Description                                                        |
| --------------------------------------------- | ------------------------------------------------------------------ |
| `jalalix_set_time(j, ms)`                     | Set the UTC instant from unix milliseconds.                        |
| `jalalix_set_hours(j, hours, min, sec, ms)`   | Set wall-clock time (all subcomponents).                           |
| `jalalix_set_date(j, date)`                   | Set Jalali day of month.                                           |
| `jalalix_set_month(j, month, date)`           | Set Jalali month **0…11**; pass `date < 0` to leave day unchanged. |
| `jalalix_set_full_year(j, year, month, date)` | Set Jalali year; optional month **0…11** and day (`< 0` omits).    |
| `jalalix_set_timezone(j, tz)`                 | Instant-preserving zone change; `0` on success, `-1` if invalid.   |

### Instance - math, range, format

| API                                                               | Description                                                                                       |
| ----------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| `jalalix_add_years(j, years)`                                     | Add calendar years (mutates `j`).                                                                 |
| `jalalix_add_months(j, months)`                                   | Add calendar months (mutates `j`).                                                                |
| `jalalix_add_weeks(j, weeks)`                                     | Add weeks (mutates `j`).                                                                          |
| `jalalix_add_days(j, days)`                                       | Add days (mutates `j`).                                                                           |
| `jalalix_start_of_year(j)`                                        | Set to first moment of Jalali year (mutates `j`).                                                 |
| `jalalix_start_of_month(j)`                                       | Set to start of Jalali month.                                                                     |
| `jalalix_start_of_week(j)`                                        | Set to start of week.                                                                             |
| `jalalix_start_of_day(j)`                                         | Set to `00:00:00.000`.                                                                            |
| `jalalix_end_of_year(j)`                                          | Set to last moment of Jalali year.                                                                |
| `jalalix_end_of_month(j)`                                         | Set to end of Jalali month.                                                                       |
| `jalalix_end_of_week(j)`                                          | Set to end of week.                                                                               |
| `jalalix_end_of_day(j)`                                           | Set to `23:59:59.999`.                                                                            |
| `jalalix_format(j, fmt, out, out_len, persian_digits, gregorian)` | Format with tokens; non-zero `persian_digits` / `gregorian` flags; returns bytes written or `-1`. |
| `jalalix_to_iso_date(j, out, out_len, jalali)`                    | Write `YYYY-MM-DD` for Gregorian (`jalali == 0`) or Jalali (`jalali != 0`).                       |

## Links

- https://jalalix.com
- https://github.com/jalalix/c
- [CHANGELOG.md](./CHANGELOG.md)

## License

MIT
