﻿
#pragma once

#include <map>
#include <ctime>
#include <cstring>
#include <cctype>

#include "minizip/unzip.h"

#ifdef _MSC_VER

template<typename T>
auto strcasecmp(T a, T b) {
	return lstrcmpiA(a, b);
}

#endif

struct Res {
	const char *data;
	size_t size;
};
extern "C" Res SITE(void);

namespace{

	std::map<std::string, std::string> mime_map = {
		{ ".html", "text/html; charset=utf-8" },
		{ ".js", "application/javascript" },
		{ ".css", "text/css" },
		{ ".woff", "application/x-font-woff" },
		{ ".png", "image/png" }
	};

	struct exe_bundle_file
	{
		voidpf base_ptr;
		ZPOS64_T offset;
		uLong fsize;
		int error;

		exe_bundle_file(voidpf _base, uLong _size)
			: base_ptr(_base)
			, offset(0)
			, fsize(_size)
			, error(0)
		{}
	};

	zlib_filefunc64_def exe_bundled_file_ops =
	{
		// .zopen64_file =
		[](voidpf, const void* , int ) -> voidpf
		{
			std::size_t zip_size;
			voidpf zip_content;

			auto res = SITE();

			zip_content = const_cast<char*>(res.data);
			zip_size = res.size;

			// return membaseptr as filestream !
			auto filestream = new exe_bundle_file((voidpf)zip_content, (uLong)zip_size);
			return filestream;
		},
		// .zread_file =
		[](voidpf, voidpf stream, void* buf, uLong size) -> uLong
		{
			exe_bundle_file * f = reinterpret_cast<exe_bundle_file*>(stream);
			if (f->error == 0)
			{
				uLong copyable_size = std::min(size, uLong(f->fsize - f->offset));
				if (copyable_size > 0)
				{
					memcpy(buf, reinterpret_cast<char*>(f->base_ptr) + f->offset, copyable_size);
					f->offset += copyable_size;
					return copyable_size;
				}
				f->error = EOF;
				return 0;
			}
			return -1;
		},
		// .zwrite_file =
		[](voidpf, voidpf, const void*, uLong) -> uLong
		{
			return -1;
		},
		// .ztell64_file =
		[](voidpf, voidpf stream) -> ZPOS64_T
		{
			exe_bundle_file * f = reinterpret_cast<exe_bundle_file*>(stream);
			return f->offset;
		},
		// .zseek64_file =
		[](voidpf, voidpf stream, ZPOS64_T offset, int origin) -> long
		{
			exe_bundle_file * f = reinterpret_cast<exe_bundle_file*>(stream);

			switch (origin)
			{
			case ZLIB_FILEFUNC_SEEK_CUR:
				f->offset += offset;
				break;
			case ZLIB_FILEFUNC_SEEK_END:
				f->offset = f->fsize - offset;
				break;
			case ZLIB_FILEFUNC_SEEK_SET:
				f->offset = offset;
				break;
			default:
				return -1;
			}

			return 0;
		},
		// .zclose_file =
		[](voidpf, voidpf stream) -> int
		{
			exe_bundle_file * f = reinterpret_cast<exe_bundle_file*>(stream);
			boost::checked_delete(f);
			return 0;
		},
		// .zerror_file =
		[](voidpf, voidpf stream) -> int
		{
			exe_bundle_file * f = reinterpret_cast<exe_bundle_file*>(stream);
			return f->error;
		},
		// .opaque
		reinterpret_cast<voidpf>(0)
	};

	time_t dos2unixtime(unsigned long dostime)
	{
		struct tm ltime = { };

		ltime.tm_year = (dostime >> 25) + 80;
		ltime.tm_mon = ((dostime >> 21) & 0x0f) - 1;
		ltime.tm_mday = ((dostime >> 16) & 0x1f) +1;
		ltime.tm_hour = (dostime >> 11) & 0x0f;
		ltime.tm_min = (dostime >> 5) & 0x3f;
		ltime.tm_sec = (dostime & 0x1f) << 1;

		ltime.tm_wday = -1;
		ltime.tm_yday = -1;
		ltime.tm_isdst = -1;
		return mktime(&ltime);
	}

	std::string make_http_last_modified(std::time_t t)
	{
		tm* gmt = gmtime((const time_t*)&t);
		char time_buf[512] = { 0 };
		strftime(time_buf, 200, "%a, %d %b %Y %H:%M:%S GMT", gmt);
		return time_buf;
	}

	// this is a clone of 'struct tm' but with all fields we don't need or use cut out.
	typedef struct {
		int tm_sec;
		int tm_min;
		int tm_hour;
		int tm_mday;
		int tm_mon;
		int tm_year;
	} short_tm;

	inline time_t short_tm_time_t(short_tm *tm)
	{
		int month, year, leap_days;
		static const int month_days_cumulative[12] = {
			0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
		};

		// we don't support years before 1970 as they will cause this function
		// to return a negative value.
		if (tm->tm_year < 70)
			return -1;

		year = tm->tm_year + 1900;
		month = tm->tm_mon;
		if (month < 0)
		{
			year += (11 - month) / 12;
			month = 11 - (11 - month) % 12;
		}
		else if (month >= 12)
		{
			year -= month / 12;
			month = month % 12;
		}

		leap_days = year - (tm->tm_mon <= 1);
		leap_days = ((leap_days / 4) - (leap_days / 100) + (leap_days / 400)
			- (1969 / 4) + (1969 / 100) - (1969 / 400));

		return ((((time_t)(year - 1970) * 365
			+ leap_days + month_days_cumulative[month] + tm->tm_mday - 1) * 24
			+ tm->tm_hour) * 60 + tm->tm_min) * 60 + tm->tm_sec;
	}

	static const char * const wkday[] = {
		"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
	};
	static const char * const weekday[] = {
		"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
	};
	static const char * const month[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	typedef struct {
		char name[5];
		int offset;		/* +/- in minutes */
	} tzinfo;
	#define TMP_DAYZONE -60
	static const tzinfo tz[] = {
		{ "GMT", 0 },              /* Greenwich Mean */
		{ "UTC", 0 },              /* Universal (Coordinated) */
		{ "WET", 0 },              /* Western European */
		{ "BST", 0 TMP_DAYZONE },  /* British Summer */
		{ "WAT", 60 },             /* West Africa */
		{ "AST", 240 },            /* Atlantic Standard */
		{ "ADT", 240 TMP_DAYZONE },/* Atlantic Daylight */
		{ "EST", 300 },            /* Eastern Standard */
		{ "EDT", 300 TMP_DAYZONE },/* Eastern Daylight */
		{ "CST", 360 },            /* Central Standard */
		{ "CDT", 360 TMP_DAYZONE },/* Central Daylight */
		{ "MST", 420 },            /* Mountain Standard */
		{ "MDT", 420 TMP_DAYZONE },/* Mountain Daylight */
		{ "PST", 480 },            /* Pacific Standard */
		{ "PDT", 480 TMP_DAYZONE },/* Pacific Daylight */
		{ "YST", 540 },            /* Yukon Standard */
		{ "YDT", 540 TMP_DAYZONE },/* Yukon Daylight */
		{ "HST", 600 },            /* Hawaii Standard */
		{ "HDT", 600 TMP_DAYZONE },/* Hawaii Daylight */
		{ "CAT", 600 },            /* Central Alaska */
		{ "AHST", 600 },           /* Alaska-Hawaii Standard */
		{ "NT", 660 },             /* Nome */
		{ "IDLW", 720 },           /* International Date Line West */
		{ "CET", -60 },            /* Central European */
		{ "MET", -60 },            /* Middle European */
		{ "MEWT", -60 },           /* Middle European Winter */
		{ "MEST", -60 TMP_DAYZONE },  /* Middle European Summer */
		{ "CEST", -60 TMP_DAYZONE },  /* Central European Summer */
		{ "MESZ", -60 TMP_DAYZONE },  /* Middle European Summer */
		{ "FWT", -60 },               /* French Winter */
		{ "FST", -60 TMP_DAYZONE },   /* French Summer */
		{ "EET", -120 },              /* Eastern Europe, USSR Zone 1 */
		{ "WAST", -420 },             /* West Australian Standard */
		{ "WADT", -420 TMP_DAYZONE }, /* West Australian Daylight */
		{ "CCT", -480 },              /* China Coast, USSR Zone 7 */
		{ "JST", -540 },              /* Japan Standard, USSR Zone 8 */
		{ "EAST", -600 },             /* Eastern Australian Standard */
		{ "EADT", -600 TMP_DAYZONE }, /* Eastern Australian Daylight */
		{ "GST", -600 },              /* Guam Standard, USSR Zone 9 */
		{ "NZT", -720 },              /* New Zealand */
		{ "NZST", -720 },             /* New Zealand Standard */
		{ "NZDT", -720 TMP_DAYZONE }, /* New Zealand Daylight */
		{ "IDLE", -720 },             /* International Date Line East */
		/* Next up: Military timezone names. RFC822 allowed these, but (as noted in
		RFC 1123) had their signs wrong. Here we use the correct signs to match
		actual military usage.
		*/
		{ "A", +1 * 60 },         /* Alpha */
		{ "B", +2 * 60 },         /* Bravo */
		{ "C", +3 * 60 },         /* Charlie */
		{ "D", +4 * 60 },         /* Delta */
		{ "E", +5 * 60 },         /* Echo */
		{ "F", +6 * 60 },         /* Foxtrot */
		{ "G", +7 * 60 },         /* Golf */
		{ "H", +8 * 60 },         /* Hotel */
		{ "I", +9 * 60 },         /* India */
		/* "J", Juliet is not used as a timezone, to indicate the observer's local
		time */
		{ "K", +10 * 60 },        /* Kilo */
		{ "L", +11 * 60 },        /* Lima */
		{ "M", +12 * 60 },        /* Mike */
		{ "N", -1 * 60 },         /* November */
		{ "O", -2 * 60 },         /* Oscar */
		{ "P", -3 * 60 },         /* Papa */
		{ "Q", -4 * 60 },         /* Quebec */
		{ "R", -5 * 60 },         /* Romeo */
		{ "S", -6 * 60 },         /* Sierra */
		{ "T", -7 * 60 },         /* Tango */
		{ "U", -8 * 60 },         /* Uniform */
		{ "V", -9 * 60 },         /* Victor */
		{ "W", -10 * 60 },        /* Whiskey */
		{ "X", -11 * 60 },        /* X-ray */
		{ "Y", -12 * 60 },        /* Yankee */
		{ "Z", 0 },               /* Zulu, zero meridian, a.k.a. UTC */
	};

	typedef enum {
		DATE_MDAY,
		DATE_YEAR,
		DATE_TIME
	} assume;

	namespace detail {
		inline int sltosi(long slnum)
		{
			const long curl_mask_sint = 0x7FFFFFFF;
			return (int)(slnum & (long)curl_mask_sint);
		}
		inline void skip(const char** date)
		{
			// skip everything that aren't letters or digits.
			while (**date && !std::isalnum(**date))
				(*date)++;
		}

		inline int checkday(const char *check, size_t len)
		{
			int i;
			const char * const *what;
			bool found = false;

			if (len > 3)
				what = &weekday[0];
			else
				what = &wkday[0];
			for (i = 0; i < 7; i++)
			{
				if (strcasecmp(check, what[0]) == 0)
				{
					found = true;
					break;
				}
				what++;
			}
			return found ? i : -1;
		}

		inline int checkmonth(const char *check)
		{
			int i;
			const char * const *what;
			bool found = false;

			what = &month[0];
			for (i = 0; i < 12; i++)
			{
				if (strcasecmp(check, what[0]) == 0)
				{
					found = true;
					break;
				}
				what++;
			}

			// return the offset or -1, no real offset is -1.
			return found ? i : -1;
		}

		// return the time zone offset between GMT and the input one, in number
		// of seconds or -1 if the timezone wasn't found/legal
		inline int checktz(const char *check)
		{
			unsigned int i;
			const tzinfo *what;
			bool found = false;

			what = tz;
			for (i = 0; i < sizeof(tz) / sizeof(tz[0]); i++)
			{
				if (strcasecmp(check, what->name) == 0)
				{
					found = true;
					break;
				}
				what++;
			}

			return found ? what->offset * 60 : -1;
		}

	}

	bool parse_gmt_time_fmt(const char* date, time_t* output)
	{
		time_t t = 0;
		// day of the week number, 0-6 (mon-sun)
		int wdaynum = -1;
		// month of the year number, 0-11
		int monnum = -1;
		// day of month, 1 - 31
		int mdaynum = -1;
		int hournum = -1;
		int minnum = -1;
		int secnum = -1;
		int yearnum = -1;
		int tzoff = -1;
		short_tm tm;
		assume dignext = DATE_MDAY;
		// save the original pointer
		const char *indate = date;
		// max 6 parts
		int part = 0;

		while (*date && (part < 6))
		{
			bool found = false;

			detail::skip(&date);

			if (std::isalpha(*date))
			{
				// a name coming up
				char buf[32] = "";
				size_t len;

				sscanf(date, "%31[ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz]", buf);
				len = strlen(buf);

				if (wdaynum == -1)
				{
					wdaynum = detail::checkday(buf, len);
					if (wdaynum != -1)
						found = true;
				}

				if (!found && (monnum == -1))
				{
					monnum = detail::checkmonth(buf);
					if (monnum != -1)
						found = true;
				}

				if (!found && (tzoff == -1))
				{
					// this just must be a time zone string.
					tzoff = detail::checktz(buf);
					if (tzoff != -1)
						found = true;
				}

				// bad string.
				if (!found)
					return false;

				date += len;
			}
			else if (std::isdigit(*date))
			{
				// a digit.
				int val;
				char *end;
				// time stamp!
				if ((secnum == -1) && (3 == sscanf(date, "%02d:%02d:%02d", &hournum, &minnum, &secnum)))
					date += 8;
				else if ((secnum == -1) && (2 == sscanf(date, "%02d:%02d", &hournum, &minnum)))
				{
					// time stamp without seconds.
					date += 5;
					secnum = 0;
				}
				else
				{
					val = detail::sltosi(strtol(date, &end, 10));

					if ((tzoff == -1) &&
						((end - date) == 4) &&
						(val <= 1400) &&
						(indate < date) &&
						((date[-1] == '+' || date[-1] == '-')))
					{
						// four digits and a value less than or equal to 1400 (to take into
						// account all sorts of funny time zone diffs) and it is preceded
						// with a plus or minus. This is a time zone indication.  1400 is
						// picked since +1300 is frequently used and +1400 is mentioned as
						// an edge number in the document "ISO C 200X Proposal: Timezone
						// Functions" at http://david.tribble.com/text/c0xtimezone.html If
						// anyone has a more authoritative source for the exact maximum time
						// zone offsets, please speak up!
						found = true;
						tzoff = (val / 100 * 60 + val % 100) * 60;

						// the + and - prefix indicates the local time compared to GMT
						// this we need ther reversed math to get what we want.
						tzoff = date[-1] == '+' ? -tzoff : tzoff;
					}

					if (((end - date) == 8) &&
						(yearnum == -1) &&
						(monnum == -1) &&
						(mdaynum == -1))
					{
						// 8 digits, no year, month or day yet. This is YYYYMMDD
						found = true;
						yearnum = val / 10000;
						// month is 0 - 11.
						monnum = (val % 10000) / 100 - 1;
						mdaynum = val % 100;
					}

					if (!found && (dignext == DATE_MDAY) && (mdaynum == -1))
					{
						if ((val > 0) && (val < 32))
						{
							mdaynum = val;
							found = true;
						}
						dignext = DATE_YEAR;
					}

					if (!found && (dignext == DATE_YEAR) && (yearnum == -1))
					{
						yearnum = val;
						found = true;
						if (yearnum < 1900)
						{
							if (yearnum > 70)
								yearnum += 1900;
							else
								yearnum += 2000;
						}
						if (mdaynum == -1)
							dignext = DATE_MDAY;
					}

					if (!found)
						return false;

					date = end;
				}
			}

			part++;
		}

		if (-1 == secnum)
			secnum = minnum = hournum = 0; // no time, make it zero.

		// lacks vital info, fail.
		if ((-1 == mdaynum) || (-1 == monnum) || (-1 == yearnum))
			return false;

	#if SIZEOF_TIME_T < 5
		// 32 bit time_t can only hold dates to the beginning of 2038
		if (yearnum > 2037)
		{
			*output = 0x7fffffff;
			return false;
		}
	#endif

		if (yearnum < 1970)
		{
			*output = 0;
			return false;
		}

		tm.tm_sec = secnum;
		tm.tm_min = minnum;
		tm.tm_hour = hournum;
		tm.tm_mday = mdaynum;
		tm.tm_mon = monnum;
		tm.tm_year = yearnum - 1900;

		// short_tm_time_t() returns a time_t. time_t is often 32 bits, even on many
		// architectures that feature 64 bit 'long'.
		// Some systems have 64 bit time_t and deal with years beyond 2038. However,
		// even on some of the systems with 64 bit time_t mktime() returns -1 for
		// dates beyond 03:14:07 UTC, January 19, 2038. (Such as AIX 5100-06)

		t = short_tm_time_t(&tm);

		// time zone adjust (cast t to int to compare to negative one)
		if (-1 != (int)t)
		{
			// Add the time zone diff between local time zone and GMT.
			long delta = (long)(tzoff != -1 ? tzoff : 0);
			// time_t overflow
			if ((delta > 0) && (t + delta < t))
				return false;

			t += delta;
		}

		*output = t;

		return true;
	}

}