/* Copyright (C) 2014 J.F.Dockes
 *       This program is free software; you can redistribute it and/or modify
 *       it under the terms of the GNU General Public License as published by
 *       the Free Software Foundation; either version 2 of the License, or
 *       (at your option) any later version.
 *
 *       This program is distributed in the hope that it will be useful,
 *       but WITHOUT ANY WARRANTY; without even the implied warranty of
 *       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *       GNU General Public License for more details.
 *
 *       You should have received a copy of the GNU General Public License
 *       along with this program; if not, write to the
 *       Free Software Foundation, Inc.,
 *       59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef _AVLASTCHG_H_X_INCLUDED_
#define _AVLASTCHG_H_X_INCLUDED_

#include <string>                       // for string
#include <unordered_map>                // for unordered_map

namespace UPnPClient {
/** Decoding AV LastChange data
 *    <Event xmlns="urn:schemas-upnp-org:metadata-1-0/AVT_RCS">
 *      <InstanceID val="0">
 *        <Mute val="0"/>
 *        <Volume val="24"/>
 *      </InstanceID>
 *    </Event>
 */
extern bool decodeAVLastChange(const std::string& xml,
                               std::unordered_map<std::string, std::string>& props);


} // namespace UPnPClient

#endif /* _AVLASTCHG_H_X_INCLUDED_ */
