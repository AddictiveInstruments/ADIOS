/*
	Copyright 2001, 2002 Georges Menie (www.menie.org)
	stdarg version contributed by Christian Ettinger

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
	putchar is the only external dependency for this file,
	if you have a working putchar, leave it commented out.
	If not, uncomment the define below and
	replace outbyte(c) by your own function call.

#define putchar(c) outbyte(c)
*/


#include <stdarg.h>
#include <adios.h>

/* THIS FAMILY NOW ONLY FORMATS INTO BUFFERS.
 *
 * printchar() used to have a second exit: when no buffer was given, it sent
 * the character to the serial console with ADIOS_COM_SendChar(1, c). That
 * transport - adios_com.c - was removed on 2026-08-13, so the console exit
 * went with it and printf()/vprintf() are now silent: they still parse the
 * format and still return the character count, they simply have nowhere to
 * put the result.
 *
 * sprintf() and vsprintf() are UNAFFECTED and are what everything actually
 * uses: the MIDI layer's answers to host queries, the LCD layer's screen
 * printf, and application code.
 *
 * To get console output back, give printchar() an exit again here: one line
 * calling whatever transport that project actually has.
 */
static void printchar(char **str, int c)
{
	if (str) {
		**str = c;
		++(*str);
	}
}

#ifdef BSL_USE_REDUCED_SPRINTF

/* A format parser cut down to what a bootloader needs. Defined by the
 * bootloader's own adios_config.h and by nothing else - every other build
 * compiles the full parser further down and is not affected in any way, the
 * switch being absent from its preprocessor.
 *
 * WHY IT EXISTS: a bootloader's size decides where the application starts,
 * because the boundary is rounded up to the flash erase granularity - a whole
 * 16K sector on some families. The full parser costs about 600 bytes to
 * answer four host queries which between them use "%d" and "%08x".
 *
 * WHAT IT UNDERSTANDS: "%d" (signed decimal), "%x" (lower-case hex), each
 * with optional zero padding written "%0<width>"; "%%"; and literal text.
 *
 * WHAT IT DOES NOT: "%s", "%X", "%u", "%c", left justification, blank
 * padding. Those emit '?' instead of consuming their argument silently, so a
 * format this build cannot serve is VISIBLE in the answer rather than
 * swallowed - add the specifier here, or drop the switch for that build.
 */
static int print( char **out, const char *format, va_list args )
{
	char buf[12];			/* 10 digits of a 32 bit value, plus NUL */
	register int pc = 0;
	int width, base, neg, len, value;
	unsigned u;
	char *s;

	for (; *format != 0; ++format) {

		if( *format != '%' ) {
			printchar (out, *format);
			++pc;
			continue;
		}

		++format;
		if( *format == '\0' ) break;

		if( *format == '%' ) {
			printchar (out, '%');
			++pc;
			continue;
		}

		width = 0;
		if( *format == '0' ) {
			++format;
			for ( ; *format >= '0' && *format <= '9'; ++format)
				width = 10*width + (*format - '0');
		}

		if( *format == 'd' || *format == 'x' ) {
			base = (*format == 'x') ? 16 : 10;
			value = va_arg( args, int );
			u = (unsigned)value;
			neg = 0;

			if( base == 10 && value < 0 ) {
				neg = 1;
				u = (unsigned)(-value);
			}

			s = buf + sizeof(buf) - 1;
			*s = '\0';
			do {
				unsigned t = u % base;
				*--s = (t < 10) ? ('0' + t) : ('a' + t - 10);
				u /= base;
			} while( u );

			if( neg ) {		/* sign first, then the zeros */
				printchar (out, '-');
				++pc;
				if( width ) --width;
			}

			for (len = (buf + sizeof(buf) - 1) - s; len < width; ++len) {
				printchar (out, '0');
				++pc;
			}

			while( *s ) {
				printchar (out, *s++);
				++pc;
			}
			continue;
		}

		printchar (out, '?');	/* see the comment above */
		++pc;
	}

	if (out) **out = '\0';
	va_end( args );
	return pc;
}

#else /* !BSL_USE_REDUCED_SPRINTF - the full parser */

#define PAD_RIGHT 1
#define PAD_ZERO 2

static int prints(char **out, const char *string, int width, int pad)
{
	register int pc = 0, padchar = ' ';

	if (width > 0) {
		register int len = 0;
		register const char *ptr;
		for (ptr = string; *ptr; ++ptr) ++len;
		if (len >= width) width = 0;
		else width -= len;
		if (pad & PAD_ZERO) padchar = '0';
	}
	if (!(pad & PAD_RIGHT)) {
		for ( ; width > 0; --width) {
			printchar (out, padchar);
			++pc;
		}
	}
	for ( ; *string ; ++string) {
		printchar (out, *string);
		++pc;
	}
	for ( ; width > 0; --width) {
		printchar (out, padchar);
		++pc;
	}

	return pc;
}

#ifdef PRINT_SUPPORT_BINARY
#define PRINT_BUF_LEN 33
#else
/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 12
#endif

static int printi(char **out, int i, int b, int sg, int width, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	register char *s;
	register int t, neg = 0, pc = 0;
	register unsigned int u = i;

	if (i == 0) {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints (out, print_buf, width, pad);
	}

	if (sg && b == 10 && i < 0) {
		neg = 1;
		u = -i;
	}

	s = print_buf + PRINT_BUF_LEN-1;
	*s = '\0';

	while (u) {
		t = u % b;
		if( t >= 10 )
			t += letbase - '0' - 10;
		*--s = t + '0';
		u /= b;
	}

	if (neg) {
		if( width && (pad & PAD_ZERO) ) {
			printchar (out, '-');
			++pc;
			--width;
		}
		else {
			*--s = '-';
		}
	}

	return pc + prints (out, s, width, pad);
}

static int print( char **out, const char *format, va_list args )
{
	register int width, pad;
	register int pc = 0;
	char scr[2];

	for (; *format != 0; ++format) {
		if (*format == '%') {
			++format;
			width = pad = 0;
			if (*format == '\0') break;
			if (*format == '%') goto out;
			if (*format == '-') {
				++format;
				pad = PAD_RIGHT;
			}
			while (*format == '0') {
				++format;
				pad |= PAD_ZERO;
			}
			for ( ; *format >= '0' && *format <= '9'; ++format) {
				width *= 10;
				width += *format - '0';
			}
			if( *format == 's' ) {
				register char *s = (char *)va_arg( args, int );
				pc += prints (out, s?s:"(null)", width, pad);
				continue;
			}
			if( *format == 'd' ) {
				pc += printi (out, va_arg( args, int ), 10, 1, width, pad, 'a');
				continue;
			}
			if( *format == 'x' ) {
				pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'a');
				continue;
			}
			if( *format == 'X' ) {
				pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'A');
				continue;
			}
			if( *format == 'u' ) {
				pc += printi (out, va_arg( args, int ), 10, 0, width, pad, 'a');
				continue;
			}
			#ifdef PRINT_SUPPORT_BINARY
			if( *format == 'b' ) {
				pc += printi (out, va_arg( args, int ), 2, 0, width, pad, 'A');
				continue;
			}
			#endif
			if( *format == 'c' ) {
				/* char are converted to int then pushed on the stack */
				scr[0] = (char)va_arg( args, int );
				scr[1] = '\0';
				pc += prints (out, scr, width, pad);
				continue;
			}
		}
		else {
		out:
			printchar (out, *format);
			++pc;
		}
	}
	if (out) **out = '\0';
	va_end( args );
	return pc;
}

#endif /* BSL_USE_REDUCED_SPRINTF */

int printf(const char *format, ...)
{
        va_list args;
        
        va_start( args, format );
        return print( 0, format, args );
}

// TK: added for alternative parameter passing
int vprintf(const char *format, va_list args)
{
  return print( 0, format, args );
}

int sprintf(char *out, const char *format, ...)
{
        va_list args;
        
        va_start( args, format );
        return print( &out, format, args );
}

// TK: added for alternative parameter passing
int vsprintf(char *out, const char *format, va_list args)
{
  char *_out;
  _out = out;
  return print( &_out, format, args );
}


/* snprintf() WAS DEFINED HERE AND HAS BEEN REMOVED (2026-08-13).
 *
 * Do not "restore" it. What stood here was:
 *
 *     int snprintf( char *buf, size_t count, const char *format, ... )
 *     {
 *             va_list args;
 *             ( void ) count;                          // <-- the bound, discarded
 *             va_start( args, format );
 *             return print( &buf, format, args );
 *     }
 *
 * i.e. exactly sprintf() under a name that promises a bound it never
 * honoured. Nothing in the tree called it, so nothing was protected - but
 * the day someone reached for it precisely BECAUSE the name says "safe",
 * they would have got a silent overrun instead of a diagnostic.
 *
 * This is not an oversight of ours: the file comes from the FreeRTOS demo
 * directories, and FreeRTOS itself says so in Source/tasks.c - "note
 * printf-stdarg.c does not provide a full snprintf() implementation!". The
 * warning simply did not travel with the file.
 *
 * Removing the symbol turns any future use into a link error at the exact
 * moment the mistake is made, which is the whole point. Bounding it for
 * real is possible but not free: print() writes through a char** and
 * printchar() knows no limit, so an end pointer would have to be threaded
 * through printchar/prints/printi/print - four signatures and a dozen call
 * sites in the very code that every sprintf() in the tree depends on. A
 * static counter instead would be six lines and NOT reentrant, which in an
 * OS that formats from interrupt context trades one trap for a worse one.
 *
 * If a bounded formatter is ever needed, write it under its own name -
 * something like ADIOS_SPRINTF_Bounded() - rather than borrowing a
 * standard one.
 */


#ifdef TEST_PRINTF
int main(void)
{
	char *ptr = "Hello world!";
	char *np = 0;
	int i = 5;
	unsigned int bs = sizeof(int)*8;
	int mi;
	char buf[80];

	mi = (1 << (bs-1)) + 1;
	printf("%s\n", ptr);
	printf("printf test\n");
	printf("%s is null pointer\n", np);
	printf("%d = 5\n", i);
	printf("%d = - max int\n", mi);
	printf("char %c = 'a'\n", 'a');
	printf("hex %x = ff\n", 0xff);
	printf("hex %02x = 00\n", 0);
	printf("signed %d = unsigned %u = hex %x\n", -3, -3, -3);
	printf("%d %s(s)%", 0, "message");
	printf("\n");
	printf("%d %s(s) with %%\n", 0, "message");
	sprintf(buf, "justif: \"%-10s\"\n", "left"); printf("%s", buf);
	sprintf(buf, "justif: \"%10s\"\n", "right"); printf("%s", buf);
	sprintf(buf, " 3: %04d zero padded\n", 3); printf("%s", buf);
	sprintf(buf, " 3: %-4d left justif.\n", 3); printf("%s", buf);
	sprintf(buf, " 3: %4d right justif.\n", 3); printf("%s", buf);
	sprintf(buf, "-3: %04d zero padded\n", -3); printf("%s", buf);
	sprintf(buf, "-3: %-4d left justif.\n", -3); printf("%s", buf);
	sprintf(buf, "-3: %4d right justif.\n", -3); printf("%s", buf);

	return 0;
}

/*
 * if you compile this file with
 *   gcc -Wall $(YOUR_C_OPTIONS) -DTEST_PRINTF -c printf.c
 * you will get a normal warning:
 *   printf.c:214: warning: spurious trailing `%' in format
 * this line is testing an invalid % at the end of the format string.
 *
 * this should display (on 32bit int machine) :
 *
 * Hello world!
 * printf test
 * (null) is null pointer
 * 5 = 5
 * -2147483647 = - max int
 * char a = 'a'
 * hex ff = ff
 * hex 00 = 00
 * signed -3 = unsigned 4294967293 = hex fffffffd
 * 0 message(s)
 * 0 message(s) with %
 * justif: "left      "
 * justif: "     right"
 *  3: 0003 zero padded
 *  3: 3    left justif.
 *  3:    3 right justif.
 * -3: -003 zero padded
 * -3: -3   left justif.
 * -3:   -3 right justif.
 */

#endif


