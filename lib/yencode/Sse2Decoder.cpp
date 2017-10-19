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

namespace Sse2
{
#ifdef __SSE2__
#define SIMD_DECODER
#include "SimdDecoder.cpp"
#endif
}

void init_decode_sse2() {
#ifdef __SSE2__
	decode = &YEncode::Sse2::do_decode_simd<sizeof(__m128i), YEncode::Sse2::do_decode_sse<false>>;
	YEncode::Sse2::decoder_init();
	decode_simd = true;
#endif
}

}
