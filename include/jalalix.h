/**
 * JalaliX — Jalali (Persian / Shamsi) calendar library
 * Pure C99, standard library only.
 *
 * Copyright (c) Hossein Maklavani
 * MIT License
 */

#ifndef JALALIX_H
#define JALALIX_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

    /** Opaque JalaliX date/time instance (Jalali calendar view over an epoch timestamp). */
    typedef struct jalalix jalalix_t;

    /**
     * Creates a JalaliX instance from the current local date and time.
     * @return New jalalix_t* (caller must jalalix_free), or NULL on failure.
     */
    jalalix_t *jalalix_create_now(void);

    /**
     * Creates a JalaliX instance from Unix milliseconds.
     * @param ms Milliseconds since 1970-01-01 UTC.
     * @return New jalalix_t* (caller must jalalix_free), or NULL on failure.
     */
    jalalix_t *jalalix_create_ms(int64_t ms);

    /**
     * Creates a JalaliX instance from a Gregorian ISO date string.
     * Accepts `YYYY-MM-DD` or `YYYY-MM-DDTHH:MM:SS[.sss][Z]`.
     * @param iso Gregorian ISO date/time string.
     * @return New jalalix_t* (caller must jalalix_free), or NULL on failure.
     */
    jalalix_t *jalalix_create_iso(const char *iso);

    /**
     * Frees a JalaliX instance previously returned by a create/parse/clone function.
     * @param j Instance to free; NULL is a no-op.
     */
    void jalalix_free(jalalix_t *j);

    /**
     * Parses a Jalali date string into a JalaliX instance.
     * Accepts `YYYY/MM/DD` or `YYYY-MM-DD`, optional time (`HH:mm` / `HH:mm:ss`),
     * and Persian digits (e.g. "۱۴۰۳-۱۲-۰۷ 14:30").
     * @param value Jalali date string.
     * @return New jalalix_t* (caller must jalalix_free), or NULL on failure.
     */
    jalalix_t *jalalix_parse_jalali(const char *value);

    /**
     * Formats the date according to the specified format string into `out` (NUL-terminated on success).
     * Tokens match TypeScript JalaliX.format (e.g. "YYYY/MM/DD", "DD MMM YYYY").
     * @param j Source instance.
     * @param fmt Format string.
     * @param out Output buffer.
     * @param out_len Capacity of `out` in bytes (including space for NUL).
     * @param persian_digits Non-zero -> convert numerals to Persian digits (۰-۹).
     * @param gregorian Non-zero -> use Gregorian calendar tokens; otherwise Jalali.
     * @return Bytes written (excluding NUL), or -1 on error.
     */
    int jalalix_format(const jalalix_t *j, const char *fmt, char *out, size_t out_len, int persian_digits,
                       int gregorian);

    /**
     * Gets the Jalali year.
     * @param j Source instance.
     * @return The year (e.g. 1403).
     */
    int jalalix_get_full_year(const jalalix_t *j);

    /**
     * Gets the Jalali month (1–12; Farvardin = 1).
     * @param j Source instance.
     * @return The month number (1–12).
     */
    int jalalix_get_month(const jalalix_t *j);

    /**
     * Gets the day of the month in the Jalali calendar (1–31).
     * @param j Source instance.
     * @return The day of the month.
     */
    int jalalix_get_date(const jalalix_t *j);

    /**
     * Gets the day of the week, matching TypeScript JalaliX.getDay()
     * (1 = Saturday … 7 = Friday in Jalali week numbering).
     * @param j Source instance.
     * @return The day number (1–7).
     */
    int jalalix_get_day(const jalalix_t *j);

    /**
     * Gets the hours value using local time (0–23).
     * @param j Source instance.
     * @return The hours value.
     */
    int jalalix_get_hours(const jalalix_t *j);

    /**
     * Gets the minutes value using local time (0–59).
     * @param j Source instance.
     * @return The minutes value.
     */
    int jalalix_get_minutes(const jalalix_t *j);

    /**
     * Gets the seconds value using local time (0–59).
     * @param j Source instance.
     * @return The seconds value.
     */
    int jalalix_get_seconds(const jalalix_t *j);

    /**
     * Gets the milliseconds value using local time (0–999).
     * @param j Source instance.
     * @return The milliseconds value.
     */
    int jalalix_get_milliseconds(const jalalix_t *j);

    /**
     * Gets the Unix timestamp in milliseconds.
     * @param j Source instance.
     * @return Milliseconds since 1970-01-01 UTC.
     */
    int64_t jalalix_get_time(const jalalix_t *j);

    /**
     * Sets the date and time from a Unix millisecond timestamp.
     * @param j Instance to mutate.
     * @param ms Milliseconds since 1970-01-01 UTC.
     * @return The new timestamp in milliseconds (like Date.setTime).
     */
    int64_t jalalix_set_time(jalalix_t *j, int64_t ms);

    /**
     * Sets the hour (and optionally minutes, seconds, milliseconds) using local time.
     * @param j Instance to mutate.
     * @param hours Hours value (0–23).
     * @param min Minutes value.
     * @param sec Seconds value.
     * @param ms Milliseconds value.
     * @return The new timestamp in milliseconds.
     */
    int64_t jalalix_set_hours(jalalix_t *j, int hours, int min, int sec, int ms);

    /**
     * Sets the numeric day-of-the-month in the Jalali calendar.
     * @param j Instance to mutate.
     * @param date Day of the month (1–31).
     * @return The new timestamp in milliseconds.
     */
    int64_t jalalix_set_date(jalalix_t *j, int date);

    /**
     * Sets the Jalali month (0–11; Farvardin = 0), optionally with a day of month.
     * @param j Instance to mutate.
     * @param month Zero-based month (0 = Farvardin … 11 = Esfand).
     * @param date Day of the month; pass `date < 0` to leave the day unchanged.
     * @return The new timestamp in milliseconds.
     */
    int64_t jalalix_set_month(jalalix_t *j, int month, int date);

    /**
     * Sets the Jalali year, optionally month and day of month.
     * @param j Instance to mutate.
     * @param year Jalali year.
     * @param month Zero-based month when provided (0 = Farvardin); pass `month < 0` to omit.
     * @param date Day of the month when provided; pass `date < 0` to omit.
     * @return The new timestamp in milliseconds.
     */
    int64_t jalalix_set_full_year(jalalix_t *j, int year, int month, int date);

    /**
     * Adds the specified number of years to the date (can be negative).
     * @param j Instance to mutate.
     * @param years Number of years to add.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_add_years(jalalix_t *j, int years);

    /**
     * Adds the specified number of months to the date (can be negative).
     * @param j Instance to mutate.
     * @param months Number of months to add.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_add_months(jalalix_t *j, int months);

    /**
     * Adds the specified number of weeks to the date (can be negative).
     * @param j Instance to mutate.
     * @param weeks Number of weeks to add.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_add_weeks(jalalix_t *j, int weeks);

    /**
     * Adds the specified number of days to the date (can be negative).
     * @param j Instance to mutate.
     * @param days Number of days to add.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_add_days(jalalix_t *j, int days);

    /**
     * Sets the instance to the start of the Jalali year (first moment of Farvardin 1).
     * @param j Instance to mutate.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_start_of_year(jalalix_t *j);

    /**
     * Sets the instance to the start of the current Jalali month.
     * @param j Instance to mutate.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_start_of_month(jalalix_t *j);

    /**
     * Sets the instance to the start of the current week.
     * @param j Instance to mutate.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_start_of_week(jalalix_t *j);

    /**
     * Sets the instance to the start of the current day (00:00:00.000).
     * @param j Instance to mutate.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_start_of_day(jalalix_t *j);

    /**
     * Sets the instance to the end of the Jalali year (last moment of Esfand).
     * @param j Instance to mutate.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_end_of_year(jalalix_t *j);

    /**
     * Sets the instance to the end of the current Jalali month.
     * @param j Instance to mutate.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_end_of_month(jalalix_t *j);

    /**
     * Sets the instance to the end of the current week.
     * @param j Instance to mutate.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_end_of_week(jalalix_t *j);

    /**
     * Sets the instance to the end of the current day (23:59:59.999).
     * @param j Instance to mutate.
     * @return The same `j` pointer after mutation.
     */
    jalalix_t *jalalix_end_of_day(jalalix_t *j);

    /**
     * Creates a copy of this JalaliX instance with the same timestamp.
     * @param j Source instance.
     * @return New jalalix_t* (caller must jalalix_free), or NULL on failure.
     */
    jalalix_t *jalalix_clone(const jalalix_t *j);

    /**
     * Writes a calendar date string `YYYY-MM-DD` into `out` (local date; no UTC shift).
     * @param j Source instance.
     * @param out Output buffer.
     * @param out_len Capacity of `out` in bytes (including space for NUL).
     * @param jalali Non-zero -> Jalali calendar; otherwise Gregorian.
     * @return 0 on success, -1 on error.
     */
    int jalalix_to_iso_date(const jalalix_t *j, char *out, size_t out_len, int jalali);

    /**
     * Converts Western numerals (0–9) to Persian numerals (۰–۹) in a string.
     * @param in Input string containing Western numerals.
     * @param out Output buffer.
     * @param out_len Capacity of `out` in bytes (including space for NUL).
     * @return 0 on success, -1 on error.
     */
    int jalalix_to_persian(const char *in, char *out, size_t out_len);

    /**
     * Converts Persian numerals (۰–۹) to Western numerals (0–9) in a string.
     * @param in Input string containing Persian numerals.
     * @param out Output buffer.
     * @param out_len Capacity of `out` in bytes (including space for NUL).
     * @return 0 on success, -1 on error.
     */
    int jalalix_from_persian(const char *in, char *out, size_t out_len);

    /**
     * Compares two JalaliX dates (matches TypeScript JalaliX.compare).
     * @param a First date.
     * @param b Second date to compare against.
     * @return 1 if `a` is before `b`, else 0.
     */
    int jalalix_compare(const jalalix_t *a, const jalalix_t *b);

    /**
     * Sets the IANA timezone on this instance (instant-preserving).
     * The UTC millisecond instant is unchanged; wall clock / Jalali day may change.
     * @param j Instance to mutate.
     * @param tz IANA id (e.g. "Asia/Tehran"), or alias utc/gmt/local/default/system.
     * @return 0 on success, -1 if `tz` is invalid / unknown.
     */
    int jalalix_set_timezone(jalalix_t *j, const char *tz);

    /**
     * Gets the canonical IANA timezone id (e.g. "Asia/Tehran" or "UTC").
     * @param j Source instance.
     * @return Pointer to internal NUL-terminated string (valid until next set/free), or NULL.
     */
    const char *jalalix_get_timezone(const jalalix_t *j);

#ifdef __cplusplus
}
#endif

#endif
