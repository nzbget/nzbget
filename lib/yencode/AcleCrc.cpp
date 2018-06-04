/*
 *  Based on node-yencode library by Anime Tosho:
 *  https://github.com/animetosho/node-yencode
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

// inspired off https://github.com/jocover/crc32_armv8/blob/master/crc32_armv8.c

#include "nzbget.h"

#include "YEncode.h"

#ifdef __ARM_FEATURE_CRC32
#include <arm_acle.h>
#endif

namespace YEncode
{
#ifdef __ARM_FEATURE_CRC32

void crc_arm_init(crc_state *const s)
{
	s->crc0[0] = ~0;
}

void crc_arm(crc_state *const s, const unsigned char *src, long len) {
	uint32_t crc = s->crc0[0];

	// initial alignment
	if (len >= 16) { // 16 is an arbitrary number; it just needs to be >=8
		if ((uintptr_t)src & sizeof(uint8_t)) {
			crc = __crc32b(crc, *src);
			src++;
			len--;
		}
		if ((uintptr_t)src & sizeof(uint16_t)) {
			crc = __crc32h(crc, *((uint16_t *)src));
			src += sizeof(uint16_t);
			len -= sizeof(uint16_t);
		}
		
#ifdef __aarch64__
		if ((uintptr_t)src & sizeof(uint32_t)) {
			crc = __crc32w(crc, *((uint32_t *)src));
			src += sizeof(uint32_t);
			len -= sizeof(uint32_t);
		}
	}
	while ((len -= sizeof(uint64_t)) >= 0) {
		crc = __crc32d(crc, *((uint64_t *)src));
		src += sizeof(uint64_t);
	}
	if (len & sizeof(uint32_t)) {
		crc = __crc32w(crc, *((uint32_t *)src));
		src += sizeof(uint32_t);
	}
#else
	}
	while ((len -= sizeof(uint32_t)) >= 0) {
		crc = __crc32w(crc, *((uint32_t *)src));
		src += sizeof(uint32_t);
	}
#endif
	if (len & sizeof(uint16_t)) {
		crc = __crc32h(crc, *((uint16_t *)src));
		src += sizeof(uint16_t);
	}
	if (len & sizeof(uint8_t))
		crc = __crc32b(crc, *src);
	
	s->crc0[0] = crc;
}

uint32_t crc_arm_finish(crc_state *const s)
{
	return ~s->crc0[0];
}
#endif

void init_crc_acle()
{
#ifdef __ARM_FEATURE_CRC32
	crc_init = &crc_arm_init;
	crc_incr = &crc_arm;
	crc_finish = &crc_arm_finish;
	crc_simd = true;
#endif
}

}
