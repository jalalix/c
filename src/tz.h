// Internal TZif / IANA timezone helpers for JalaliX (not part of public API).
#ifndef JALALIX_TZ_H
#define JALALIX_TZ_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct
	{
		const char *id;
		const unsigned char *data;
		size_t len;
	} jx_tz_embed_t;

	// NULL-terminated table of embedded TZif blobs (from tzdata_embed.c).
	extern const jx_tz_embed_t jx_tz_embedded[];

	/**
	 * Resolve timezone input to a canonical IANA id.
	 * @param input NULL -> copy current; aliases utc/gmt/local/default/system supported.
	 * @param current Fallback when input is NULL (must be non-NULL).
	 * @param out Destination buffer.
	 * @param out_len Capacity of out including NUL.
	 * @return 0 on success, -1 if invalid / unknown zone.
	 */
	int jx_tz_resolve(const char *input, const char *current, char *out, size_t out_len);

	/**
	 * UTC offset (seconds east of UTC) for unix_ms in iana_id.
	 * @return 0 on success, -1 on failure.
	 */
	int jx_tz_offset_seconds(const char *iana_id, int64_t unix_ms, int32_t *out_offset);

	// wall_ms = unix_ms + offset*1000. Returns 0 / -1.
	int jx_tz_utc_to_wall_ms(const char *iana_id, int64_t unix_ms, int64_t *out_wall);

	// Convert wall civil time in zone to UTC unix ms. Returns 0 / -1.
	int jx_tz_wall_to_utc_ms(const char *iana_id, int y, int m, int d, int h, int mi, int s, int ms,
							 int64_t *out_utc);

	// Detect host local IANA id (static storage; never NULL - falls back to "UTC").
	const char *jx_tz_detect_host(void);

#ifdef __cplusplus
}
#endif

#endif
