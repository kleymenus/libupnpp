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
#ifndef _MEDIARENDERER_HXX_INCLUDED_
#define _MEDIARENDERER_HXX_INCLUDED_

#include <memory>                       // for shared_ptr
#include <string>                       // for string
#include <vector>                       // for vector

#include "libupnpp/control/avtransport.hxx"  // for AVTH
#include "libupnpp/control/device.hxx"  // for Device
#include "libupnpp/control/ohplaylist.hxx"  // for OHPLH
#include "libupnpp/control/ohproduct.hxx"  // for OHPRH
#include "libupnpp/control/renderingcontrol.hxx"  // for RDCH

namespace UPnPClient {

class MediaRenderer;
class UPnPDeviceDesc;

typedef std::shared_ptr<MediaRenderer> MRDH;

class MediaRenderer : public Device {
public:
    MediaRenderer(const UPnPDeviceDesc& desc);

    RDCH rdc();
    AVTH avt();
    OHPRH ohpr();
    OHPLH ohpl();

    bool hasOpenHome();

    static bool getDeviceDescs(std::vector<UPnPDeviceDesc>& devices,
                               const std::string& friendlyName = "");
    static bool isMRDevice(const std::string& devicetype);

protected:
    RDCH m_rdc;
    AVTH m_avt;
    OHPRH m_ohpr;
    OHPLH m_ohpl;

    static const std::string DType;
};

}

#endif /* _MEDIARENDERER_HXX_INCLUDED_ */
