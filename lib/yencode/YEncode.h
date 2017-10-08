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
extern size_t (*decode)(const unsigned char* inbuf, unsigned char* outbuf, size_t, char* state);
extern size_t (*decode_simd)(const unsigned char* inbuf, unsigned char* outbuf, size_t, char* state);
size_t decode_scalar(const unsigned char* src, unsigned char* dest, size_t len, char* state);
extern uint32_t (*crc32_simd)(const unsigned char* src, long len);
extern uint32_t (*inc_crc32_simd)(uint32_t crc, const unsigned char* src, long len);

}

#endif
