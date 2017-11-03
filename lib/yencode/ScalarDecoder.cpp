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

#include "YEncode.h"


namespace YEncode
{

// return values:
// - 0: no end sequence found
// - 1: \r\n=y sequence found, src points to byte after 'y'
// - 2: \r\n.\r\n sequence found, src points to byte after last '\n'
int decode_scalar(const unsigned char** src, unsigned char** dest, size_t len, YencDecoderState* state) {
	const unsigned char *es = (*src) + len; // end source pointer
	unsigned char *p = *dest; // destination pointer
	long i = -(long)len; // input position
	unsigned char c; // input character
	
	if(len < 1) return 0;
	
#define YDEC_CHECK_END(s) if(i == 0) { \
	*state = s; \
	*src = es; \
	*dest = p; \
	return 0; \
}
	if(state) switch(*state) {
		case YDEC_STATE_CRLFEQ: do_decode_endable_scalar_ceq:
			if(es[i] == 'y') {
				*state = YDEC_STATE_NONE;
				*src = es+i+1;
				*dest = p;
				return 1;
			} // else fall thru and escape
		case YDEC_STATE_EQ:
			c = es[i];
			*p++ = c - 42 - 64;
			i++;
			if(c != '\r') break;
			YDEC_CHECK_END(YDEC_STATE_CR)
			// fall through
		case YDEC_STATE_CR:
			if(es[i] != '\n') break;
			i++;
			YDEC_CHECK_END(YDEC_STATE_CRLF)
		case YDEC_STATE_CRLF: do_decode_endable_scalar_c0:
			if(es[i] == '.') {
				i++;
				YDEC_CHECK_END(YDEC_STATE_CRLFDT)
				// fallthru
			} else if(es[i] == '=') {
				i++;
				YDEC_CHECK_END(YDEC_STATE_CRLFEQ)
				goto do_decode_endable_scalar_ceq;
			} else
				break;
		case YDEC_STATE_CRLFDT:
			if(es[i] == '\r') {
				i++;
				YDEC_CHECK_END(YDEC_STATE_CRLFDTCR)
				// fallthru
			} else if(es[i] == '=') { // check for dot-stuffed ending: \r\n.=y
				i++;
				YDEC_CHECK_END(YDEC_STATE_CRLFEQ)
				goto do_decode_endable_scalar_ceq;
			} else
				break;
		case YDEC_STATE_CRLFDTCR:
			if(es[i] == '\n') {
				*state = YDEC_STATE_CRLF;
				*src = es + i + 1;
				*dest = p;
				return 2;
			} else
				break;
		case YDEC_STATE_NONE: break; // silence compiler warning
	} else // treat as YDEC_STATE_CRLF
		goto do_decode_endable_scalar_c0;
	
	for(; i < -2; i++) {
		c = es[i];
		switch(c) {
			case '\r': {
				if(es[i+1] == '\n') {
					c = es[i+2];
					if(c == '.') {
						// skip past \r\n. sequences
						i += 3;
						YDEC_CHECK_END(YDEC_STATE_CRLFDT)
						// check for end
						if(es[i] == '\r') {
							i++;
							YDEC_CHECK_END(YDEC_STATE_CRLFDTCR)
							if(es[i] == '\n') {
								*src = es + i + 1;
								*dest = p;
								*state = YDEC_STATE_CRLF;
								return 2;
							} else i--;
						} else if(es[i] == '=') {
							i++;
							YDEC_CHECK_END(YDEC_STATE_CRLFEQ)
							if(es[i] == 'y') {
								*src = es + i + 1;
								*dest = p;
								*state = YDEC_STATE_NONE;
								return 1;
							} else {
								// escape char & continue
								c = es[i];
								*p++ = c - 42 - 64;
								i -= (c == '\r');
							}
						} else i--;
					}
					else if(c == '=') {
						i += 3;
						YDEC_CHECK_END(YDEC_STATE_CRLFEQ)
						if(es[i] == 'y') {
							// ended
							*src = es + i + 1;
							*dest = p;
							*state = YDEC_STATE_NONE;
							return 1;
						} else {
							// escape char & continue
							c = es[i];
							*p++ = c - 42 - 64;
							i -= (c == '\r');
						}
					}
				}
			} case '\n':
				continue;
			case '=':
				c = es[i+1];
				*p++ = c - 42 - 64;
				i += (c != '\r'); // if we have a \r, reprocess character to deal with \r\n. case
				continue;
			default:
				*p++ = c - 42;
		}
	}
	if(state) *state = YDEC_STATE_NONE;
	
	if(i == -2) { // 2nd last char
		c = es[i];
		switch(c) {
			case '\r':
				if(state && es[i+1] == '\n') {
					*state = YDEC_STATE_CRLF;
					*src = es;
					*dest = p;
					return 0;
				}
			case '\n':
				break;
			case '=':
				c = es[i+1];
				*p++ = c - 42 - 64;
				i += (c != '\r');
				break;
			default:
				*p++ = c - 42;
		}
		i++;
	}
	
	// do final char; we process this separately to prevent an overflow if the final char is '='
	if(i == -1) {
		c = es[i];
		if(c != '\n' && c != '\r' && c != '=') {
			*p++ = c - 42;
		} else if(state) {
			if(c == '=') *state = YDEC_STATE_EQ;
			else if(c == '\r') *state = YDEC_STATE_CR;
			else *state = YDEC_STATE_NONE;
		}
	}
#undef YDEC_CHECK_END
	
	*src = es;
	*dest = p;
	return 0;
}

void init_decode_scalar() {
	decode = decode_scalar;
}

}
