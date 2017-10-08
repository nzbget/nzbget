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

#ifdef __SSE2__
#include <immintrin.h>
#endif

namespace YEncode
{
#ifdef __SSE2__

// combine two 8-bit ints into a 16-bit one
#define UINT16_PACK(a, b) ((a) | ((b) << 8))

#define XMM_SIZE 16 /*== (signed int)sizeof(__m128i)*/

#define STOREU_XMM(dest, xmm) \
  _mm_storeu_si128((__m128i*)(dest), xmm)

#define LOAD_HALVES(a, b) _mm_castps_si128(_mm_loadh_pi( \
	_mm_castsi128_ps(_mm_loadl_epi64((__m128i*)(a))), \
	(b) \
))

uint8_t eqFixLUT[256];
alignas(32) __m64 eqAddLUT[256];

size_t do_decode_sse2(const unsigned char* src, unsigned char* dest, size_t len, char* state) {
	if(len <= sizeof(__m128i)*2) return decode_scalar(src, dest, len, state);
	
	unsigned char *p = dest; // destination pointer
	unsigned long i = 0; // input position
	unsigned char escFirst = 0; // input character; first char needs escaping
	unsigned int nextMask = 0;
	char tState = 0;
	char* pState = state ? state : &tState;
	if((uintptr_t)src & ((sizeof(__m128i)-1))) {
		// find source memory alignment
		unsigned char* aSrc = (unsigned char*)(((uintptr_t)src + (sizeof(__m128i)-1)) & ~(sizeof(__m128i)-1));
		
		i = aSrc - src;
		p += decode_scalar(src, dest, i, pState);
	}
	
	if(*pState == 0 && i+1 < len && src[i] == '.')
		nextMask = 1;
	else if(*pState == 2 && i+2 < len && *(uint16_t*)(src + i) == UINT16_PACK('\n','.'))
		nextMask = 2;

	escFirst = *pState == 1;
	
	if(i + (sizeof(__m128i)+1) < len) {
		// our algorithm may perform an aligned load on the next part, of which we consider 2 bytes (for \r\n. sequence checking)
		size_t dLen = len - (sizeof(__m128i)+1);
		dLen = ((dLen-i) + 0xf) & ~0xf;
		unsigned char* dSrc = (unsigned char*)src + dLen + i;
		long dI = -dLen;
		i += dLen;
		
		for(; dI; dI += sizeof(__m128i)) {
			__m128i data = _mm_load_si128((__m128i *)(dSrc + dI));
			
			// search for special chars
			__m128i cmpEq = _mm_cmpeq_epi8(data, _mm_set1_epi8('=')),
			cmp = _mm_or_si128(
				_mm_or_si128(
					_mm_cmpeq_epi8(data, _mm_set1_epi16(0x0a0d)), // \r\n
					_mm_cmpeq_epi8(data, _mm_set1_epi16(0x0d0a))  // \n\r
				),
				cmpEq
			);

			unsigned int mask = _mm_movemask_epi8(cmp); // not the most accurate mask if we have invalid sequences; we fix this up later
			
			__m128i oData;
			if(escFirst) { // rarely hit branch: seems to be faster to use 'if' than a lookup table, possibly due to values being able to be held in registers?
				// first byte needs escaping due to preceeding = in last loop iteration
				oData = _mm_sub_epi8(data, _mm_set_epi8(42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42+64));
			} else {
				oData = _mm_sub_epi8(data, _mm_set1_epi8(42));
			}
			mask &= ~escFirst;
			mask |= nextMask;
			
			if (mask != 0) {
				// a spec compliant encoder should never generate sequences: ==, =\n and =\r, but we'll handle them to be spec compliant
				// the yEnc specification requires any character following = to be unescaped, not skipped over, so we'll deal with that

				// firstly, resolve invalid sequences of = to deal with cases like '===='
				unsigned int maskEq = _mm_movemask_epi8(cmpEq);
				unsigned int tmp = eqFixLUT[(maskEq&0xff) & ~escFirst];
				maskEq = (eqFixLUT[(maskEq>>8) & ~(tmp>>7)] << 8) | tmp;
				
				escFirst = (maskEq >> (sizeof(__m128i)-1));
				// next, eliminate anything following a `=` from the special char mask; this eliminates cases of `=\r` so that they aren't removed
				maskEq <<= 1;
				mask &= ~maskEq;
				
				// unescape chars following `=`
				oData = _mm_add_epi8(
					oData,
					LOAD_HALVES(
						eqAddLUT + (maskEq&0xff),
						eqAddLUT + ((maskEq>>8)&0xff)
					)
				);

				// handle \r\n. sequences
				// RFC3977 requires the first dot on a line to be stripped, due to dot-stuffing
				// find instances of \r\n
				__m128i tmpData1, tmpData2;
				tmpData1 = _mm_insert_epi16(_mm_srli_si128(data, 1), *(uint16_t*)(dSrc + dI + sizeof(__m128i)-1), 7);
				tmpData2 = _mm_insert_epi16(_mm_srli_si128(data, 2), *(uint16_t*)(dSrc + dI + sizeof(__m128i)), 7);
				__m128i cmp1 = _mm_cmpeq_epi16(data, _mm_set1_epi16(0x0a0d));
				__m128i cmp2 = _mm_cmpeq_epi16(tmpData1, _mm_set1_epi16(0x0a0d));
				// prepare to merge the two comparisons
				cmp1 = _mm_srli_si128(cmp1, 1);
				// find all instances of .
				tmpData2 = _mm_cmpeq_epi8(tmpData2, _mm_set1_epi8('.'));
				// merge matches of \r\n with those for .
				unsigned int killDots = _mm_movemask_epi8(
					_mm_and_si128(tmpData2, _mm_or_si128(cmp1, cmp2))
				);
				mask |= (killDots << 2) & 0xffff;
				nextMask = killDots >> (sizeof(__m128i)-2);

				// all that's left is to 'compress' the data (skip over masked chars)
				alignas(32) uint32_t mmTmp[4];
				_mm_store_si128((__m128i*)mmTmp, oData);
				
				for(int j=0; j<4; j++) {
					if(mask & 0xf) {
						unsigned char* pMmTmp = (unsigned char*)(mmTmp + j);
						unsigned int maskn = ~mask;
						*p = pMmTmp[0];
						p += (maskn & 1);
						*p = pMmTmp[1];
						p += (maskn & 2) >> 1;
						*p = pMmTmp[2];
						p += (maskn & 4) >> 2;
						*p = pMmTmp[3];
						p += (maskn & 8) >> 3;
					} else {
						*(uint32_t*)p = mmTmp[j];
						p += 4;
					}
					mask >>= 4;
				}
			} else {
				STOREU_XMM(p, oData);
				p += XMM_SIZE;
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

extern size_t (*decode_sse2)(const unsigned char* src, unsigned char* dest, size_t len, char* state);
#endif

void init_decode_sse2() {
#ifdef __SSE2__
	decode_sse2 = &do_decode_sse2;

	// generate unshuf LUT
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
		_mm_storel_epi64((__m128i*)(eqAddLUT + i), _mm_loadl_epi64((__m128i*)res));
	}
#endif
}

}
