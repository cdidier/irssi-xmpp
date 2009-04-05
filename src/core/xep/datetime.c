/*
 * $Id$
 *
 * Copyright (C) 2009 Colin DIDIER
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * XEP-0082: XMPP Date and Time Profiles
 */

#include <ctype.h>
#include <time.h>

#define FORMAT  "%Y-%m-%dT%T"

static long
parse_timezone(const char *tz)
{
	const char *rfc822_timezones[26][4] = {
		{ "M", NULL },			/* UTC-12 */
		{ "L", NULL },
		{ "K", NULL },
		{ "I", NULL },
		{ "H", "PST", NULL }, 		/* UTC-8 */
		{ "G", "MST", "PDT", NULL },	/* UTC-7 */
		{ "F", "CST", "MDT", NULL },	/* UTC-6 */
		{ "E", "EST", "CDT", NULL },	/* UTC-5 */
		{ "D", "EDT", NULL },		/* UTC-4 */
		{ "C", NULL },
		{ "B", NULL },
		{ "A", NULL },
		{ "Z", "UT", "GMT", NULL },	/* UTC */
		{ "N", NULL },
		{ "O", NULL },
		{ "P", NULL },
		{ "Q", NULL },
		{ "R", NULL },
		{ "S", NULL },
		{ "T", NULL },
		{ "U", NULL },
		{ "V", NULL },
		{ "W", NULL },
		{ "X", NULL },
		{ "Y", NULL },			/* UTC+12 */
		{ NULL }
	};
	long i, j;

	if ((*tz == '+' || *tz == '-') && strlen(tz) == 5) {
		i = atoi(tz);
		return ((i/100)*60 + i%100) * 60;
	}
	for (i = 0; rfc822_timezones[i] != NULL; ++i)
		for (j = 0; rfc822_timezones[i][j] != NULL; ++j)
			if (strcmp(rfc822_timezones[i][j], tz) == 0)
				return (i - 12) * 3600;
	return 0;
}

time_t
xep82_datetime(const char *stamp)
{
	struct tm tm;
	long offset;
	char *s;

	memset(&tm, 0, sizeof(struct tm));
	if ((s = strptime(stamp, FORMAT, &tm)) == NULL)
		return (time_t)-1;
	/* ignore fractional second addendum */
	if (*s++ == '.')
		while (isdigit(*s++));
	tm.tm_isdst = -1;
	tm.tm_gmtoff = offset = *s != '\0' ? parse_timezone(s) : 0;
	return mktime(&tm) - offset;
}
