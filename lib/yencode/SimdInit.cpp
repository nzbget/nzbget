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

#if (defined(__i686__) || defined(__amd64__)) && !defined(WIN32)
#include <cpuid.h>
#endif

#include "YEncode.h"

namespace YEncode
{

int (*decode)(const unsigned char**, unsigned char**, size_t, YencDecoderState*) = nullptr;
extern void init_decode_scalar();
bool decode_simd = false;

void (*crc_init)(crc_state *const s) = nullptr;
void (*crc_incr)(crc_state *const s, const unsigned char *src, long len) = nullptr;
uint32_t (*crc_finish)(crc_state *const s) = nullptr;
extern void init_crc_slice();
bool crc_simd = false;

#if defined(__i686__) || defined(__amd64__)
extern void init_decode_sse2();
extern void init_decode_ssse3();
extern void init_crc_pclmul();

class CpuId
{
	uint32_t regs[4];
public:
	CpuId(unsigned level)
	{
#ifdef WIN32
		__cpuid((int *)regs, (int)level);
#else
		__cpuid(level, regs[0], regs[1], regs[2], regs[3]);
#endif
	}
	const uint32_t &EAX() const {return regs[0];}
	const uint32_t &EBX() const {return regs[1];}
	const uint32_t &ECX() const {return regs[2];}
	const uint32_t &EDX() const {return regs[3];}
};
#endif

#if defined(__arm__) || defined(__aarch64__)
extern void init_decode_neon();
extern void init_crc_acle();
#endif

void init()
{
	init_decode_scalar();
	init_crc_slice();

#if defined(__i686__) || defined(__amd64__)
	CpuId cpuid(1);

	bool cpu_supports_sse2 = cpuid.EDX() & 0x04000000;
	bool cpu_supports_ssse3 = cpuid.ECX() & 0x00000200;
	bool cpu_supports_sse41 = cpuid.ECX() & 0x00080000;
	bool cpu_supports_pclmul = cpuid.ECX() & 0x00000002;

	if (cpu_supports_sse2)
	{
		init_decode_sse2();
	}
	if (cpu_supports_ssse3)
	{
		init_decode_ssse3();
	}
	if (cpu_supports_sse41 && cpu_supports_pclmul)
	{
		init_crc_pclmul();
	}
#endif

#if defined(__arm__) || defined(__aarch64__)
	bool cpu_supports_neon = false;
	bool cpu_supports_crc = false;

#ifdef __linux__
	if (FILE* file = fopen("/proc/cpuinfo", "r"))
	{
		char buf[200];
		while (fgets(buf, sizeof(buf), file))
		{
			cpu_supports_neon |= !strncasecmp(buf, "Features", 8) &&
				(strstr(buf, " neon ") || strstr(buf, " asimd "));
			cpu_supports_crc |= !strncasecmp(buf, "Features", 8) && strstr(buf, " crc32 ");
		}
		fclose(file);
	}
#endif

	if (cpu_supports_neon)
	{
		init_decode_neon();
	}
	if (cpu_supports_crc)
	{
		init_crc_acle();
	}
#endif
}

}
