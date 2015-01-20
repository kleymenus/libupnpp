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

#include "libupnpp/control/device.hxx"

#include "libupnpp/log.hxx"
#include "libupnpp/control/description.hxx"

using namespace std;
using namespace UPnPP;

namespace UPnPClient {


class Device::Internal {
public:
    UPnPDeviceDesc desc;
};


Device::Device() 
{
    if ((m = new Internal()) == 0) {
        LOGERR("Device::Device: out of memory" << endl);
        return;
    }
}

Device::Device(const UPnPDeviceDesc& desc)
{
    if ((m = new Internal()) == 0) {
        LOGERR("Device::Device: out of memory" << endl);
        return;
    }
    m->desc = desc;
}


const UPnPDeviceDesc *Device::desc() const
{
    return m ? &m->desc : 0;
}

}
