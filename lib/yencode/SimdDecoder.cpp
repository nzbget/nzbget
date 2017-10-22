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


#ifdef SIMD_DECODER

// combine two 8-bit ints into a 16-bit one
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define UINT16_PACK(a, b) ((a) | ((b) << 8))
#define UINT32_PACK(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#else
#define UINT16_PACK(a, b) (((a) << 8) | (b))
#define UINT32_PACK(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
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

template<int width, void kernel(size_t&, const uint8_t*, unsigned char*&, unsigned char&, uint16_t&)>
int do_decode_simd(const unsigned char** src, unsigned char** dest, size_t len, YencDecoderState* state) {
	if(len <= width*2) return decode_scalar(src, dest, len, state);
	
	YencDecoderState tState = YDEC_STATE_CRLF;
	YencDecoderState* pState = state ? state : &tState;
	if((uintptr_t)(*src) & ((width-1))) {
		// find source memory alignment
		unsigned char* aSrc = (unsigned char*)(((uintptr_t)(*src) + (width-1)) & ~(width-1));
		int amount = (int)(aSrc - *src);
		len -= amount;
		int ended = decode_scalar(src, dest, amount, pState);
		if(ended) return ended;
	}
	
	size_t lenBuffer = width -1;
	lenBuffer += 3 + 1;

	if(len > lenBuffer) {
		unsigned char *p = *dest; // destination pointer
		unsigned char escFirst = 0; // input character; first char needs escaping
		uint16_t nextMask = 0;
		// handle finicky case of special sequences straddled across initial boundary
		switch(*pState) {
			case YDEC_STATE_CRLF:
				if(**src == '.') {
					nextMask = 1;
					if(*(uint16_t*)(*src +1) == UINT16_PACK('\r','\n')) {
						(*src) += 3;
						*pState = YDEC_STATE_CRLF;
						return 2;
					}
					if(*(uint16_t*)(*src +1) == UINT16_PACK('=','y')) {
						(*src) += 3;
						*pState = YDEC_STATE_NONE;
						return 1;
					}
				}
				else if(*(uint16_t*)(*src) == UINT16_PACK('=','y')) {
					(*src) += 2;
					*pState = YDEC_STATE_NONE;
					return 1;
				}
				break;
			case YDEC_STATE_CR:
				if(*(uint16_t*)(*src) == UINT16_PACK('\n','.')) {
					nextMask = 2;
					if(*(uint16_t*)(*src +2) == UINT16_PACK('\r','\n')) {
						(*src) += 4;
						*pState = YDEC_STATE_CRLF;
						return 2;
					}
					if(*(uint16_t*)(*src +2) == UINT16_PACK('=','y')) {
						(*src) += 4;
						*pState = YDEC_STATE_NONE;
						return 1;
					}
				}
				else if((*(uint32_t*)(*src) & 0xffffff) == UINT32_PACK('\n','=','y',0)) {
					(*src) += 3;
					*pState = YDEC_STATE_NONE;
					return 1;
				}
				break;
			case YDEC_STATE_CRLFDT:
				if(*(uint16_t*)(*src) == UINT16_PACK('\r','\n')) {
					(*src) += 2;
					*pState = YDEC_STATE_CRLF;
					return 2;
				}
				if(*(uint16_t*)(*src) == UINT16_PACK('=','y')) {
					(*src) += 2;
					*pState = YDEC_STATE_NONE;
					return 1;
				}
				break;
			case YDEC_STATE_CRLFDTCR:
				if(**src == '\n') {
					(*src) += 1;
					*pState = YDEC_STATE_CRLF;
					return 2;
				}
				break;
			case YDEC_STATE_CRLFEQ:
				if(**src == 'y') {
					(*src) += 1;
					*pState = YDEC_STATE_NONE;
					return 1;
				}
				break;
			default: break; // silence compiler warning
		}
		escFirst = (*pState == YDEC_STATE_EQ || *pState == YDEC_STATE_CRLFEQ);
		
		// our algorithm may perform an aligned load on the next part, of which we consider 2 bytes (for \r\n. sequence checking)
		size_t dLen = len - lenBuffer;
		dLen = (dLen + (width-1)) & ~(width-1);
		const uint8_t* dSrc = (const uint8_t*)(*src) + dLen;

		kernel(dLen, dSrc, p, escFirst, nextMask);

		if(escFirst) *pState = YDEC_STATE_EQ; // escape next character
		else if(nextMask == 1) *pState = YDEC_STATE_CRLF; // next character is '.', where previous two were \r\n
		else if(nextMask == 2) *pState = YDEC_STATE_CR; // next characters are '\n.', previous is \r
		else *pState = YDEC_STATE_NONE;
		
		*src += dLen;
		len -= dLen;
		*dest = p;
	}
	
	// end alignment
	if(len)
		return decode_scalar(src, dest, len, pState);

	return 0;
}

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

#ifdef __SSE2__

#define XMM_SIZE 16 /*== (signed int)sizeof(__m128i)*/

#if defined(__tune_core2__) || defined(__tune_atom__)
/* on older Intel CPUs, plus first gen Atom, it is faster to store XMM registers in half */
# define STOREU_XMM(dest, xmm) \
  _mm_storel_epi64((__m128i*)(dest), xmm); \
  _mm_storeh_pi(((__m64*)(dest) +1), _mm_castsi128_ps(xmm))
#else
# define STOREU_XMM(dest, xmm) \
  _mm_storeu_si128((__m128i*)(dest), xmm)
#endif

#define LOAD_HALVES(a, b) _mm_castps_si128(_mm_loadh_pi( \
	_mm_castsi128_ps(_mm_loadl_epi64((__m128i*)(a))), \
	(b) \
))

uint8_t eqFixLUT[256];
alignas(32) __m64 eqAddLUT[256];
#ifdef __SSSE3__
alignas(32) __m64 unshufLUT[256];
#endif

template<bool use_ssse3>
static inline void do_decode_sse(size_t& dLen, const uint8_t* dSrc, unsigned char*& p, unsigned char& escFirst, uint16_t& nextMask) {
	long dI = -(long)dLen;

	for(; dI; dI += sizeof(__m128i)) {
		const uint8_t* src = dSrc + dI;

		__m128i data = _mm_load_si128((__m128i *)src);
		
		// search for special chars
		__m128i cmpEq = _mm_cmpeq_epi8(data, _mm_set1_epi8('=')),
#ifdef __AVX512VL__
		cmp = _mm_ternarylogic_epi32(
			_mm_cmpeq_epi8(data, _mm_set1_epi16(0x0a0d)),
			_mm_cmpeq_epi8(data, _mm_set1_epi16(0x0d0a)),
			cmpEq,
			0xFE
		);
#else
		cmp = _mm_or_si128(
			_mm_or_si128(
				_mm_cmpeq_epi8(data, _mm_set1_epi16(0x0a0d)), // \r\n
				_mm_cmpeq_epi8(data, _mm_set1_epi16(0x0d0a))  // \n\r
			),
			cmpEq
		);
#endif
		uint16_t mask = _mm_movemask_epi8(cmp); // not the most accurate mask if we have invalid sequences; we fix this up later
		
		__m128i oData;
		if(escFirst) { // rarely hit branch: seems to be faster to use 'if' than a lookup table, possibly due to values being able to be held in registers?
			// first byte needs escaping due to preceeding = in last loop iteration
			oData = _mm_sub_epi8(data, _mm_set_epi8(42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42+64));
			mask &= ~1;
		} else {
			oData = _mm_sub_epi8(data, _mm_set1_epi8(42));
		}
		mask |= nextMask;
		
		if (mask != 0) {
			// a spec compliant encoder should never generate sequences: ==, =\n and =\r, but we'll handle them to be spec compliant
			// the yEnc specification requires any character following = to be unescaped, not skipped over, so we'll deal with that
			
			// firstly, resolve invalid sequences of = to deal with cases like '===='
			uint16_t maskEq = _mm_movemask_epi8(cmpEq);
			uint16_t tmp = eqFixLUT[(maskEq&0xff) & ~escFirst];
			maskEq = (eqFixLUT[(maskEq>>8) & ~(tmp>>7)] << 8) | tmp;
			
			unsigned char oldEscFirst = escFirst;
			escFirst = (maskEq >> (sizeof(__m128i)-1));
			// next, eliminate anything following a `=` from the special char mask; this eliminates cases of `=\r` so that they aren't removed
			maskEq <<= 1;
			mask &= ~maskEq;
			
			// unescape chars following `=`
#if defined(__AVX512VL__) && defined(__AVX512BW__)
			// GCC < 7 seems to generate rubbish assembly for this
			oData = _mm_mask_add_epi8(
				oData,
				maskEq,
				oData,
				_mm_set1_epi8(-64)
			);
#else
			oData = _mm_add_epi8(
				oData,
				LOAD_HALVES(
					eqAddLUT + (maskEq&0xff),
					eqAddLUT + ((maskEq>>8)&0xff)
				)
			);
#endif
			
			// handle \r\n. sequences
			// RFC3977 requires the first dot on a line to be stripped, due to dot-stuffing
			// find instances of \r\n
			__m128i tmpData1, tmpData2, tmpData3, tmpData4;
#if defined(__SSSE3__) && !defined(__tune_btver1__)
			if(use_ssse3) {
				__m128i nextData = _mm_load_si128((__m128i *)src + 1);
				tmpData1 = _mm_alignr_epi8(nextData, data, 1);
				tmpData2 = _mm_alignr_epi8(nextData, data, 2);
				tmpData3 = _mm_alignr_epi8(nextData, data, 3);
				tmpData4 = _mm_alignr_epi8(nextData, data, 4);
			} else {
#endif
				tmpData1 = _mm_insert_epi16(_mm_srli_si128(data, 1), *(uint16_t*)(src + sizeof(__m128i)-1), 7);
				tmpData2 = _mm_insert_epi16(_mm_srli_si128(data, 2), *(uint16_t*)(src + sizeof(__m128i)), 7);
				tmpData3 = _mm_insert_epi16(_mm_srli_si128(tmpData1, 2), *(uint16_t*)(src + sizeof(__m128i)+1), 7);
				tmpData4 = _mm_insert_epi16(_mm_srli_si128(tmpData2, 2), *(uint16_t*)(src + sizeof(__m128i)+2), 7);
#ifdef __SSSE3__
			}
#endif
			__m128i matchNl1 = _mm_cmpeq_epi16(data, _mm_set1_epi16(0x0a0d));
			__m128i matchNl2 = _mm_cmpeq_epi16(tmpData1, _mm_set1_epi16(0x0a0d));
			
			__m128i matchDots, matchNlDots;
			uint16_t killDots;
			matchDots = _mm_cmpeq_epi8(tmpData2, _mm_set1_epi8('.'));
			// merge preparation (for non-raw, it doesn't matter if this is shifted or not)
			matchNl1 = _mm_srli_si128(matchNl1, 1);
			
			// merge matches of \r\n with those for .
#ifdef __AVX512VL__
			matchNlDots = _mm_ternarylogic_epi32(matchDots, matchNl1, matchNl2, 0xE0);
#else
			matchNlDots = _mm_and_si128(matchDots, _mm_or_si128(matchNl1, matchNl2));
#endif
			killDots = _mm_movemask_epi8(matchNlDots);

			__m128i cmpB1 = _mm_cmpeq_epi16(tmpData2, _mm_set1_epi16(0x793d)); // "=y"
			__m128i cmpB2 = _mm_cmpeq_epi16(tmpData3, _mm_set1_epi16(0x793d));
			if(killDots) {
				// match instances of \r\n.\r\n and \r\n.=y
				__m128i cmpC1 = _mm_cmpeq_epi16(tmpData3, _mm_set1_epi16(0x0a0d)); // "\r\n"
				__m128i cmpC2 = _mm_cmpeq_epi16(tmpData4, _mm_set1_epi16(0x0a0d));
				cmpC1 = _mm_or_si128(cmpC1, cmpB2);
				cmpC2 = _mm_or_si128(cmpC2, _mm_cmpeq_epi16(tmpData4, _mm_set1_epi16(0x793d)));
				cmpC2 = _mm_slli_si128(cmpC2, 1);
				
				// prepare cmpB
				cmpB1 = _mm_and_si128(cmpB1, matchNl1);
				cmpB2 = _mm_and_si128(cmpB2, matchNl2);
				
				// and w/ dots
#ifdef __AVX512VL__
				cmpC1 = _mm_ternarylogic_epi32(cmpC1, cmpC2, matchNlDots, 0xA8);
				cmpB1 = _mm_ternarylogic_epi32(cmpB1, cmpB2, cmpC1, 0xFE);
#else
				cmpC1 = _mm_and_si128(_mm_or_si128(cmpC1, cmpC2), matchNlDots);
				cmpB1 = _mm_or_si128(cmpC1, _mm_or_si128(
					cmpB1, cmpB2
				));
#endif
			} else {
#ifdef __AVX512VL__
				cmpB1 = _mm_ternarylogic_epi32(cmpB1, matchNl1, _mm_and_si128(cmpB2, matchNl2), 0xEA);
#else
				cmpB1 = _mm_or_si128(
					_mm_and_si128(cmpB1, matchNl1),
					_mm_and_si128(cmpB2, matchNl2)
				);
#endif
			}
			if(_mm_movemask_epi8(cmpB1)) {
				// terminator found
				// there's probably faster ways to do this, but reverting to scalar code should be good enough
				escFirst = oldEscFirst;
				dLen += dI;
				return;
			}
			mask |= (killDots << 2) & 0xffff;
			nextMask = killDots >> (sizeof(__m128i)-2);

			// all that's left is to 'compress' the data (skip over masked chars)
#ifdef __SSSE3__
			if(use_ssse3) {
# if defined(__POPCNT__) && (defined(__tune_znver1__) || defined(__tune_btver2__))
				unsigned char skipped = _mm_popcnt_u32(mask & 0xff);
# else
				unsigned char skipped = BitsSetTable256[mask & 0xff];
# endif
				// lookup compress masks and shuffle
				// load up two halves
				__m128i shuf = LOAD_HALVES(unshufLUT + (mask&0xff), unshufLUT + (mask>>8));
				
				// offset upper half by 8
				shuf = _mm_add_epi8(shuf, _mm_set_epi32(0x08080808, 0x08080808, 0, 0));
				// shift down upper half into lower
				// TODO: consider using `mask & 0xff` in table instead of counting bits
				shuf = _mm_shuffle_epi8(shuf, _mm_load_si128((const __m128i*)pshufb_combine_table + skipped));
				
				// shuffle data
				oData = _mm_shuffle_epi8(oData, shuf);
				STOREU_XMM(p, oData);
				
				// increment output position
# if defined(__POPCNT__) && !defined(__tune_btver1__)
				p += XMM_SIZE - _mm_popcnt_u32(mask);
# else
				p += XMM_SIZE - skipped - BitsSetTable256[mask >> 8];
# endif
				
			} else {
#endif
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
#ifdef __SSSE3__
			}
#endif
		} else {
			STOREU_XMM(p, oData);
			p += XMM_SIZE;
			escFirst = 0;
			nextMask = 0;
		}
	}
}
#endif


#ifdef __ARM_NEON
inline uint16_t neon_movemask(uint8x16_t in) {
	uint8x16_t mask = vandq_u8(in, (uint8x16_t){1,2,4,8,16,32,64,128, 1,2,4,8,16,32,64,128});
# if defined(__aarch64__)
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

static inline void do_decode_neon(size_t& dLen, const uint8_t* dSrc, unsigned char*& p, unsigned char& escFirst, uint16_t& nextMask) {
	long dI = -(long)dLen;

	for(; dI; dI += sizeof(uint8x16_t)) {
		const uint8_t* src = dSrc + dI;

		uint8x16_t data = vld1q_u8(src);
		
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
			mask &= ~1;
		} else {
			oData = vsubq_u8(data, vdupq_n_u8(42));
		}
		mask |= nextMask;
		
		if (mask != 0) {
			// a spec compliant encoder should never generate sequences: ==, =\n and =\r, but we'll handle them to be spec compliant
			// the yEnc specification requires any character following = to be unescaped, not skipped over, so we'll deal with that
			
			// firstly, resolve invalid sequences of = to deal with cases like '===='
			uint16_t maskEq = neon_movemask(cmpEq);
			uint16_t tmp = eqFixLUT[(maskEq&0xff) & ~escFirst];
			maskEq = (eqFixLUT[(maskEq>>8) & ~(tmp>>7)] << 8) | tmp;
			
			unsigned char oldEscFirst = escFirst;
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
			uint8x16_t tmpData1, tmpData2, tmpData3, tmpData4;
			uint8x16_t nextData = vld1q_u8(src + sizeof(uint8x16_t));
			tmpData1 = vextq_u8(data, nextData, 1);
			tmpData2 = vextq_u8(data, nextData, 2);
			tmpData3 = vextq_u8(data, nextData, 3);
			tmpData4 = vextq_u8(data, nextData, 4);
			uint8x16_t matchNl1 = vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(data), vdupq_n_u16(0x0a0d)));
			uint8x16_t matchNl2 = vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(tmpData1), vdupq_n_u16(0x0a0d)));
			
			uint8x16_t matchDots, matchNlDots;
			uint16_t killDots;
			matchDots = vceqq_u8(tmpData2, vdupq_n_u8('.'));
			// merge preparation (for non-raw, it doesn't matter if this is shifted or not)
			matchNl1 = vextq_u8(matchNl1, vdupq_n_u8(0), 1);
			
			// merge matches of \r\n with those for .
			matchNlDots = vandq_u8(matchDots, vorrq_u8(matchNl1, matchNl2));
			killDots = neon_movemask(matchNlDots);

			uint8x16_t cmpB1 = vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(tmpData2), vdupq_n_u16(0x793d))); // "=y"
			uint8x16_t cmpB2 = vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(tmpData3), vdupq_n_u16(0x793d)));
			if(killDots) {
				// match instances of \r\n.\r\n and \r\n.=y
				uint8x16_t cmpC1 = vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(tmpData3), vdupq_n_u16(0x0a0d)));
				uint8x16_t cmpC2 = vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(tmpData4), vdupq_n_u16(0x0a0d)));
				cmpC1 = vorrq_u8(cmpC1, cmpB2);
				cmpC2 = vorrq_u8(cmpC2, vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(tmpData4), vdupq_n_u16(0x793d))));
				cmpC2 = vextq_u8(vdupq_n_u8(0), cmpC2, 15);
				cmpC1 = vorrq_u8(cmpC1, cmpC2);
				
				// and w/ dots
				cmpC1 = vandq_u8(cmpC1, matchNlDots);
				// then merge w/ cmpB
				cmpB1 = vandq_u8(cmpB1, matchNl1);
				cmpB2 = vandq_u8(cmpB2, matchNl2);
				
				cmpB1 = vorrq_u8(cmpC1, vorrq_u8(
					cmpB1, cmpB2
				));
			} else {
				cmpB1 = vorrq_u8(
					vandq_u8(cmpB1, matchNl1),
					vandq_u8(cmpB2, matchNl2)
				);
			}
#ifdef __aarch64__
			if(vget_lane_u64(vreinterpret_u64_u32(vqmovn_u64(vreinterpretq_u64_u8(cmpB1))), 0))
#else
			uint32x4_t tmp1 = vreinterpretq_u32_u8(cmpB1);
			uint32x2_t tmp2 = vorr_u32(vget_low_u32(tmp1), vget_high_u32(tmp1));
			if(vget_lane_u32(vpmax_u32(tmp2, tmp2), 0))
#endif
			{
				// terminator found
				// there's probably faster ways to do this, but reverting to scalar code should be good enough
				escFirst = oldEscFirst;
				dLen += dI;
				return;
			}
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
#ifdef __aarch64__
			oData = vqtbl1q_u8(oData, compact);
#else
			uint8x8x2_t dataH = {vget_low_u8(oData), vget_high_u8(oData)};
			oData = vcombine_u8(vtbl2_u8(dataH, vget_low_u8(compact)),
								vtbl2_u8(dataH, vget_high_u8(compact)));
#endif
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
}
#endif

void decoder_init() {
#ifdef __SSE2__
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

#ifdef __SSSE3__
	// generate unshuf LUT
	for(int i=0; i<256; i++) {
		int k = i;
		uint8_t res[8];
		int p = 0;
		for(int j=0; j<8; j++) {
			if(!(k & 1)) {
				res[p++] = j;
			}
			k >>= 1;
		}
		for(; p<8; p++)
			res[p] = 0;
		_mm_storel_epi64((__m128i*)(unshufLUT + i), _mm_loadl_epi64((__m128i*)res));
	}
#endif

#ifdef __ARM_NEON
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
#endif
