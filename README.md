# JalaliX

Jalali (Persian / Shamsi) calendar library in pure C99. Same algorithm as [`@jalalix/typescript`](https://github.com/jalalix/typescript): base date **1348/10/11** ↔ **1970-01-01**, leap division `0.24219858156`.

## Build

```bash
# Makefile
make
make test

# or CMake
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Quick start

```c
#include "jalalix.h"
#include <stdio.h>

int main(void) {
    jalalix_t *date = jalalix_create_iso("2025-02-25");
    char buf[64];

    jalalix_format(date, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
    // 1403/12/07

    jalalix_free(date);
    return 0;
}
```

Link with `-lm` (math). TZif data is **embedded** (`src/tzdata_embed.c`); OS `/usr/share/zoneinfo` is tried first when present.

## Timezone

Internal storage is a **UTC Unix millisecond instant** plus an IANA timezone id. `jalalix_set_timezone` is **instant-preserving**: the timestamp stays the same; wall clock / Jalali day may change.

```c
jalalix_t *d = jalalix_create_ms(1740430800000LL);

jalalix_set_timezone(d, "Asia/Tehran");
// format YYYY/MM/DD -> 1403/12/07, HH:mm:ss -> 00:30:00

jalalix_set_timezone(d, "UTC");
// format YYYY/MM/DD -> 1403/12/06, HH:mm:ss -> 21:00:00
```

- **Aliases:** `utc`/`gmt` -> `UTC`; `local`/`default`/`system` -> host zone when detectable, else `UTC`.
- **Invalid zone** -> `-1` from `jalalix_set_timezone`.
- Vendored zones: `UTC`, `Etc/UTC`, `Asia/Tehran`, `Asia/Tokyo`, `Europe/London`, `America/New_York`.

## API

Opaque handle: `typedef struct jalalix jalalix_t;`

| Function                                                              | Notes                                                           |
| --------------------------------------------------------------------- | --------------------------------------------------------------- |
| `jalalix_create_now` / `create_ms` / `create_iso`                     | Constructors (default timezone `UTC`)                           |
| `jalalix_parse_jalali`                                                | `YYYY/MM/DD` or `YYYY-MM-DD`, optional time, Persian digits     |
| `jalalix_format`                                                      | Tokens like TS; `persian_digits`, `gregorian` flags (wall time) |
| `jalalix_get_full_year` / `get_month` (1–12) / `get_date` / `get_day` | Jalali getters                                                  |
| `jalalix_set_timezone` / `get_timezone`                               | Instant-preserving IANA zone                                    |
| `jalalix_set_month` / `set_full_year`                                 | Month **0–11** (Farvardin = 0)                                  |
| `jalalix_add_*` / `start_of_*` / `end_of_*`                           | Mutate in place                                                 |
| `jalalix_to_iso_date`                                                 | Gregorian or Jalali `YYYY-MM-DD` (wall)                         |
| `jalalix_to_persian` / `from_persian`                                 | Digit conversion                                                |
| `jalalix_compare`                                                     | `1` if first is before second                                   |

## License

MIT
