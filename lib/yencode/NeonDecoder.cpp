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

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

namespace YEncode
{
#ifdef __ARM_NEON

// combine two 8-bit ints into a 16-bit one
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define UINT16_PACK(a, b) ((a) | ((b) << 8))
#else
#define UINT16_PACK(a, b) (((a) << 8) | (b))
#endif

// table from http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetTable
static const unsigned char BitsSetTable256[256] = 
{
#   define B2(n) n,     n+1,     n+1,     n+2
#   define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#   define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
    B6(0), B6(1), B6(1), B6(2)
#undef B2
#undef B4
#undef B6
};

static uint16_t neon_movemask(uint8x16_t in) {
	uint8x16_t mask = vandq_u8(in, (uint8x16_t){1,2,4,8,16,32,64,128, 1,2,4,8,16,32,64,128});
# if defined(__aarch64__) && 0
	// TODO: is this better?
	return (vaddv_u8(vget_high_u8(mask)) << 8) | vaddv_u8(vget_low_u8(mask));
# else
	uint8x8_t res = vpadd_u8(vget_low_u8(mask), vget_high_u8(mask));
	res = vpadd_u8(res, res);
	res = vpadd_u8(res, res);
	return vget_lane_u16(vreinterpret_u16_u8(res), 0);
# endif
}

uint8_t eqFixLUT[256];
alignas(32) uint8x8_t eqAddLUT[256];
alignas(32) uint8x8_t unshufLUT[256];
alignas(32) static const uint8_t pshufb_combine_table[272] = {
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x80,
	0x00,0x01,0x02,0x03,0x04,0x05,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x80,0x80,
	0x00,0x01,0x02,0x03,0x04,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x80,0x80,0x80,
	0x00,0x01,0x02,0x03,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x80,0x80,0x80,0x80,
	0x00,0x01,0x02,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x80,0x80,0x80,0x80,0x80,
	0x00,0x01,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x80,0x80,0x80,0x80,0x80,0x80,
	0x00,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
	0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
};

size_t do_decode_neon(const unsigned char* src, unsigned char* dest, size_t len, char* state) {
	if(len <= sizeof(uint8x16_t)*2) return decode_scalar(src, dest, len, state);
	
	unsigned char *p = dest; // destination pointer
	unsigned long i = 0; // input position
	unsigned char escFirst = 0; // input character; first char needs escaping
	unsigned int nextMask = 0;
	char tState = 0;
	char* pState = state ? state : &tState;
	if((uintptr_t)src & ((sizeof(uint8x16_t)-1))) {
		// find source memory alignment
		unsigned char* aSrc = (unsigned char*)(((uintptr_t)src + (sizeof(uint8x16_t)-1)) & ~(sizeof(uint8x16_t)-1));
		
		i = aSrc - src;
		p += decode_scalar(src, dest, i, pState);
	}
	
	// handle finicky case of \r\n. straddled across initial boundary
	if(*pState == 0 && i+1 < len && src[i] == '.')
		nextMask = 1;
	else if(*pState == 2 && i+2 < len && *(uint16_t*)(src + i) == UINT16_PACK('\n','.'))
		nextMask = 2;

	escFirst = *pState == 1;
	
	if(i + (sizeof(uint8x16_t)+1) < len) {
		// our algorithm may perform an aligned load on the next part, of which we consider 2 bytes (for \r\n. sequence checking)
		size_t dLen = len - (sizeof(uint8x16_t)+1);
		dLen = ((dLen-i) + 0xf) & ~0xf;
		uint8_t* dSrc = (uint8_t*)src + dLen + i;
		long dI = -dLen;
		i += dLen;
		
		for(; dI; dI += sizeof(uint8x16_t)) {
			uint8x16_t data = vld1q_u8(dSrc + dI);
			
			// search for special chars
			uint8x16_t cmpEq = vceqq_u8(data, vdupq_n_u8('=')),
			cmp = vorrq_u8(
				vorrq_u8(
					vceqq_u8(data, vreinterpretq_u8_u16(vdupq_n_u16(0x0a0d))), // \r\n
					vceqq_u8(data, vreinterpretq_u8_u16(vdupq_n_u16(0x0d0a)))  // \n\r
				),
				cmpEq
			);
			uint16_t mask = neon_movemask(cmp); // not the most accurate mask if we have invalid sequences; we fix this up later
			
			uint8x16_t oData;
			if(escFirst) { // rarely hit branch: seems to be faster to use 'if' than a lookup table, possibly due to values being able to be held in registers?
				// first byte needs escaping due to preceeding = in last loop iteration
				oData = vsubq_u8(data, (uint8x16_t){42+64,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42});
			} else {
				oData = vsubq_u8(data, vdupq_n_u8(42));
			}
			mask &= ~escFirst;
			mask |= nextMask;
			
			if (mask != 0) {
				// a spec compliant encoder should never generate sequences: ==, =\n and =\r, but we'll handle them to be spec compliant
				// the yEnc specification requires any character following = to be unescaped, not skipped over, so we'll deal with that
				
				// firstly, resolve invalid sequences of = to deal with cases like '===='
				uint16_t maskEq = neon_movemask(cmpEq);
				uint16_t tmp = eqFixLUT[(maskEq&0xff) & ~escFirst];
				maskEq = (eqFixLUT[(maskEq>>8) & ~(tmp>>7)] << 8) | tmp;
				
				escFirst = (maskEq >> (sizeof(uint8x16_t)-1));
				// next, eliminate anything following a `=` from the special char mask; this eliminates cases of `=\r` so that they aren't removed
				maskEq <<= 1;
				mask &= ~maskEq;
				
				// unescape chars following `=`
				oData = vaddq_u8(
					oData,
					vcombine_u8(
						vld1_u8((uint8_t*)(eqAddLUT + (maskEq&0xff))),
						vld1_u8((uint8_t*)(eqAddLUT + ((maskEq>>8)&0xff)))
					)
				);
				
				// handle \r\n. sequences
				// RFC3977 requires the first dot on a line to be stripped, due to dot-stuffing
				// find instances of \r\n
				uint8x16_t tmpData1, tmpData2;
				uint8x16_t nextData = vld1q_u8(dSrc + dI + sizeof(uint8x16_t));
				tmpData1 = vextq_u8(data, nextData, 1);
				tmpData2 = vextq_u8(data, nextData, 2);
				uint8x16_t cmp1 = vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(data), vdupq_n_u16(0x0a0d)));
				uint8x16_t cmp2 = vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(tmpData1), vdupq_n_u16(0x0a0d)));
				// prepare to merge the two comparisons
				cmp1 = vextq_u8(cmp1, vdupq_n_u8(0), 1);
				// find all instances of .
				tmpData2 = vceqq_u8(tmpData2, vdupq_n_u8('.'));
				// merge matches of \r\n with those for .
				uint16_t killDots = neon_movemask(
					vandq_u8(tmpData2, vorrq_u8(cmp1, cmp2))
				);
				mask |= (killDots << 2) & 0xffff;
				nextMask = killDots >> (sizeof(uint8x16_t)-2);

				// all that's left is to 'compress' the data (skip over masked chars)
				unsigned char skipped = BitsSetTable256[mask & 0xff];
				// lookup compress masks and shuffle
				oData = vcombine_u8(
					vtbl1_u8(vget_low_u8(oData),  vld1_u8((uint8_t*)(unshufLUT + (mask&0xff)))),
					vtbl1_u8(vget_high_u8(oData), vld1_u8((uint8_t*)(unshufLUT + (mask>>8))))
				);
				// compact down
				uint8x16_t compact = vld1q_u8(pshufb_combine_table + skipped*sizeof(uint8x16_t));
# ifdef __aarch64__
				oData = vqtbl1q_u8(oData, compact);
# else
				uint8x8x2_t dataH = {vget_low_u8(oData), vget_high_u8(oData)};
				oData = vcombine_u8(vtbl2_u8(dataH, vget_low_u8(compact)),
				                    vtbl2_u8(dataH, vget_high_u8(compact)));
# endif
				vst1q_u8(p, oData);
				
				// increment output position
				p += sizeof(uint8x16_t) - skipped - BitsSetTable256[mask >> 8];
				
			} else {
				vst1q_u8(p, oData);
				p += sizeof(uint8x16_t);
				escFirst = 0;
				nextMask = 0;
			}
		}
		
		if(escFirst) *pState = 1; // escape next character
		else if(nextMask == 1) *pState = 0; // next character is '.', where previous two were \r\n
		else if(nextMask == 2) *pState = 2; // next characters are '\n.', previous is \r
		else *pState = 3;
	}
	
	// end alignment
	if(i < len) {
		p += decode_scalar(src + i, p, len - i, pState);
	}
	
	return p - dest;
}

extern size_t (*decode_neon)(const unsigned char* src, unsigned char* dest, size_t len, char* state);
#endif

void init_decode_neon() {
#ifdef __ARM_NEON
	decode_neon = &do_decode_neon;

	for(int i=0; i<256; i++) {
		int k = i;
		uint8_t res[8];
		int p = 0;
		
		// fix LUT
		k = i;
		p = 0;
		for(int j=0; j<8; j++) {
			k = i >> j;
			if(k & 1) {
				p |= 1 << j;
				j++;
			}
		}
		eqFixLUT[i] = p;
		
		// sub LUT
		k = i;
		for(int j=0; j<8; j++) {
			res[j] = (k & 1) ? 192 /* == -64 */ : 0;
			k >>= 1;
		}
		vst1_u8((uint8_t*)(eqAddLUT + i), vld1_u8(res));
		
		k = i;
		p = 0;
		for(int j=0; j<8; j++) {
			if(!(k & 1)) {
				res[p++] = j;
			}
			k >>= 1;
		}
		for(; p<8; p++)
			res[p] = 0;
		vst1_u8((uint8_t*)(unshufLUT + i), vld1_u8(res));
	}
#endif
}

}
