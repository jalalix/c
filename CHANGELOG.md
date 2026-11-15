# Release Notes
All notable changes to this package will be documented in this file.

## 0.2.0
- Added `jalalix_normalize_query_date` and `jalalix_normalize_query_timestamp` (loose query dates → Gregorian `YYYY-MM-DD HH:mm:ss`; sentinels → empty; default Jalali year band `1`…`1899`)
- `jalalix_from_persian` also converts Arabic-Indic digits (`٠-٩`)
- `jalalix_parse_jalali` accepts `.` as a date separator

## 0.1.0
- Initial commit
