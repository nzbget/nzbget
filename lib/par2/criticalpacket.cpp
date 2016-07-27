//  This file is part of par2cmdline (a PAR 2.0 compatible file verification and
//  repair tool). See http://parchive.sourceforge.net for details of PAR 2.0.
//
//  Copyright (c) 2003 Peter Brian Clements
//
//  par2cmdline is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  par2cmdline is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

#include "nzbget.h"
#include "par2cmdline.h"

#ifdef _MSC_VER
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif
#endif

namespace Par2
{

bool CriticalPacket::WritePacket(DiskFile &diskfile, u64 fileoffset) const
{
  assert(packetdata != 0 && packetlength != 0);

  return diskfile.Write(fileoffset, packetdata, packetlength);
}

void CriticalPacket::FinishPacket(const MD5Hash &setid)
{
  assert(packetdata != 0 && packetlength >= sizeof(PACKET_HEADER));

  PACKET_HEADER *header = (PACKET_HEADER*)packetdata;
  header->setid = setid;

  MD5Context packetcontext;
  packetcontext.Update(&header->setid, packetlength - offsetof(PACKET_HEADER, setid));
  packetcontext.Final(header->hash);
}

} // end namespace Par2
