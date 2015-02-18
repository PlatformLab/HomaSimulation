/*
	Netvideo version 3.1
	Written by Ron Frederick <frederick@parc.xerox.com>

	Simple hack to translate a Tcl/Tk init file into a C string.
*/

/*
 * Copyright (c) Xerox Corporation 1992. All rights reserved.
 *  
 * License is granted to copy, to use, and to make and to use derivative
 * works for research and evaluation purposes, provided that Xerox is
 * acknowledged in all documentation pertaining to any such copy or derivative
 * work. Xerox grants no other licenses expressed or implied. The Xerox trade
 * name should not be used in any advertising without its written permission.
 *  
 * XEROX CORPORATION MAKES NO REPRESENTATIONS CONCERNING EITHER THE
 * MERCHANTABILITY OF THIS SOFTWARE OR THE SUITABILITY OF THIS SOFTWARE
 * FOR ANY PARTICULAR PURPOSE.  The software is provided "as is" without
 * express or implied warranty of any kind.
 *  
 * These notices must be retained in any copies of any part of this software.
 */

#include <stdio.h>
#include <string.h> /* strcasecmp() */
#include <ctype.h>
#include <stdlib.h>

/*
 * Define TCL2C_INT if your compiler has problems with long strings.
 */
#if defined(WIN32) || defined(_WIN32) || defined(__alpha__) || defined(__hpux) || defined( __APPLE__ )
#define TCL2C_INT
#endif

#ifdef _WIN32
#define strcasecmp _stricmp
#endif


/*XXX*/
void put(int c)
{
#ifdef TCL2C_INT
	static int n;

	if ((++n % 20) == 0)
		printf("%u,\n", c);
	else
		printf("%u,", c);
	/* printf("%u,%c", c, ((++n & 0xf) == 0) ? '\n' : ' '); */
#else
	switch(c) {
	case '\\':
	case '"':
		putchar('\\');
		break;
	case '\n':
		printf("\\n\\");
		break;
	default:
		break;
	}
	printf("%c", c);
#endif
}

void onefile(FILE* f, int skip_source)
{
	int nl = 1;
	int skipping = 0;
	int c;
	int look_forward = 0;

	while (1) {
		if (!look_forward)
			c = getc(f);
		else
			look_forward = 0;
		if (c == EOF)
			/* Done */
			break; 

		switch (c) {
		case ' ':
		case '\t':
		case '\f':
			if (nl || skipping)
				continue;
			break;
		case '#':
			/* 
			 * Some of TK scripts embed XBMs in them. We need 
			 * to keep the #define section there.
			 * XXX This is NOT mthread-safe!
			 */
			/* 
			 * Look ahead, read next 6 chars. If it's #define, 
			 * put the whole line; otherwise don't put anything.
			 * 
			 * We should put the read chars back, but this doesn't
			 * work on SunOS. So we have to do the trick below :( 
			 */
			if (nl) {
				look_forward = 1;
				if ((c = getc(f)) != 'd') goto next;
				if ((c = getc(f)) != 'e') goto next;
				if ((c = getc(f)) != 'f') goto next;
				if ((c = getc(f)) != 'i') goto next;
				if ((c = getc(f)) != 'n') goto next;
				if ((c = getc(f)) != 'e') goto next;
			
				/* Write '#define' and continue */
				nl = 0;
				put('#');
				put('d');
				put('e');
				put('f');
				put('i');
				put('n');
				put('e');
				look_forward = 0;
				continue;
			}

			next: 
			if (skipping)
				continue;
			if (nl) {
				skipping = 1;
				continue;
			}
			/* 
			 * Since we've looked forward and changed 'c', but we 
			 * do need to write a '#', do it here rather than 
			 * break and use the put() at the end.
			 */
			put('#');
			continue; 
		case '\n':
			if (skipping) {
				skipping = 0;
				nl = 1;
				continue;
			}
			nl = 1;
			break;
		case '\r':
		  	/* If we're reading from stdio, we might see
			 * this (^M). GCC 3.3 doesn't like processing
			 * source containing this character, so strip
			 * it out.
			 */
			continue;
		case '\\':
			if (skipping) {
				c = getc(f);
				continue;
			}
			break;
		case 's':
			if (skipping)
				continue;
			if (nl && skip_source) {
				/* skip 'source' lines */
				const char* targ = "source";
				const char* cp = targ + 1;
				while ((c = getc(f)) == *cp)
					++cp;
				if (*cp == 0) {
					if (c == ' ' || c == '\t') {
						skipping = 1;
						continue;
					}
				}
				for ( ; targ < cp; ++targ)
					put(*targ);
			}
			nl = 0;
			break;
		default:
			nl = 0;
			if (skipping)
				continue;
			break;
		}
		put(c);
	}
}

int main(int argc, char **argv)
{
	int skip_source;
	const char* name;
	
	if (argc < 2) {
		fprintf(stderr, "Usage: tcl2c++ name [ file ... ]\n");
		exit(1);
	}
#ifdef TCL2C_INT
	printf("static char code[] = {\n");
#else
	printf("static char code[] = \"");
#endif
	name = argv[1];

	/*XXX tk hack */
	skip_source = (strcasecmp(name, "et_tk") == 0) ? 1 : 0;

	if (argc == 2)
		onefile(stdin, skip_source);
	else {
		for ( ; --argc >= 2; ++argv) {
			const char* filename = argv[2];
			FILE* f = fopen(filename, "r");
			if (f == 0) {
				perror(filename);
				exit(1);
			}
			onefile(f, skip_source);
			fclose(f);
		}
	}
#ifdef TCL2C_INT
	printf("0 };\n");
#else
	printf("\";\n");
#endif
	printf("#include \"tclcl.h\"\n");
	printf("EmbeddedTcl %s(code);\n", name);
	return (0);
}
