/*
 *  Based on node-yencode library by Anime Tosho:
 *  https://github.com/animetosho/node-yencode
 *
 *  Copyright (C) 2017 Anime Tosho (animetosho)
 *  Copyright (C) 2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

namespace YEncode
{

// combine two 8-bit ints into a 16-bit one
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define UINT16_PACK(a, b) ((a) | ((b) << 8))
#else
#define UINT16_PACK(a, b) (((a) << 8) | (b))
#endif

// state var: refers to the previous state - only used for incremental processing
//   0: previous characters are `\r\n` OR there is no previous character
//   1: previous character is `=`
//   2: previous character is `\r`
//   3: previous character is none of the above
size_t decode_scalar(const unsigned char* src, unsigned char* dest, size_t len, char* state) {
	unsigned char *es = (unsigned char*)src + len; // end source pointer
	unsigned char *p = dest; // destination pointer
	long i = -(long)len; // input position
	unsigned char c; // input character

	if (len < 1) return 0;

	if (state) switch (*state) {
		case 1:
			c = es[i];
			*p++ = c - 42 - 64;
			i++;
			if (c == '\r' && i < 0) {
				*state = 2;
				// fall through to case 2
			}
			else {
				*state = 3;
				break;
			}
		case 2:
			if (es[i] != '\n') break;
			i++;
			*state = 0; // now `\r\n`
			if (i >= 0) return 0;
		case 0:
			// skip past first dot
			if (es[i] == '.') i++;
	}
	else // treat as *state == 0
		if (es[i] == '.') i++;

	for (; i < -2; i++) {
		c = es[i];
		switch (c) {
			case '\r':
				// skip past \r\n. sequences
				if (*(uint16_t*)(es + i + 1) == UINT16_PACK('\n', '.'))
					i += 2;
			case '\n':
				continue;
			case '=':
				c = es[i + 1];
				*p++ = c - 42 - 64;
				i += (c != '\r'); // if we have a \r, reprocess character to deal with \r\n. case
				continue;
			default:
				*p++ = c - 42;
		}
	}
	if (state) *state = 3;

	if (i == -2) { // 2nd last char
		c = es[i];
		switch (c) {
			case '\r':
				if (state && es[i + 1] == '\n') {
					*state = 0;
					return p - dest;
				}
			case '\n':
				break;
			case '=':
				c = es[i + 1];
				*p++ = c - 42 - 64;
				i += (c != '\r');
				break;
			default:
				*p++ = c - 42;
		}
		i++;
	}

	// do final char; we process this separately to prevent an overflow if the final char is '='
	if (i == -1) {
		c = es[i];
		if (c != '\n' && c != '\r' && c != '=') {
			*p++ = c - 42;
		}
		else if (state) {
			if (c == '=') *state = 1;
			else if (c == '\r') *state = 2;
			else *state = 3;
		}
	}

	return p - dest;
}

}
