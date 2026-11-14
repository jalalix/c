#include "jalalix.h"
#include <stdio.h>

int main(void)
{
	jalalix_t *date;
	char buf[128];

	date = jalalix_create_iso("2025-02-25");

	if (!date)
	{
		fprintf(stderr, "create_iso failed\n");
		return 1;
	}

	jalalix_format(date, "YYYY/MM/DD", buf, sizeof(buf), 0, 0);
	printf("%s\n", buf); // 1403/12/07

	jalalix_format(date, "DD MMM YYYY", buf, sizeof(buf), 1, 0);
	printf("%s\n", buf); // ۰۷ اسفند ۱۴۰۳

	jalalix_to_iso_date(date, buf, sizeof(buf), 0);
	printf("gregorian: %s\n", buf);

	jalalix_free(date);

	return 0;
}
