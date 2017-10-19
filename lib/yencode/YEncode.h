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


#ifndef YENCODE_H
#define YENCODE_H

namespace YEncode
{

void init();

typedef enum : char {
	YDEC_STATE_CRLF, // default
	YDEC_STATE_EQ,
	YDEC_STATE_CR,
	YDEC_STATE_NONE,
	YDEC_STATE_CRLFDT,
	YDEC_STATE_CRLFDTCR,
	YDEC_STATE_CRLFEQ // may actually be "\r\n.=" in raw decoder
} YencDecoderState;

extern int (*decode)(const unsigned char** src, unsigned char** dest, size_t len, YencDecoderState* state);
extern int decode_scalar(const unsigned char** src, unsigned char** dest, size_t len, YencDecoderState* state);
extern bool decode_simd;

struct crc_state
{
#if defined(__i686__) || defined(__amd64__)
	alignas(16) uint32_t crc0[4 * 5];
#else
	uint32_t crc0[1];
#endif
};

extern void (*crc_init)(crc_state *const s);
extern void (*crc_incr)(crc_state *const s, const unsigned char *src, long len);
extern uint32_t (*crc_finish)(crc_state *const s);
extern bool crc_simd;

}

#endif
