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

#include "libupnpp/control/mediarenderer.hxx"

#include <functional>                   // for _Bind, bind, _1, _2
#include <ostream>                      // for endl
#include <string>                       // for string
#include <unordered_map>                // for unordered_map, etc
#include <utility>                      // for pair
#include <vector>                       // for vector

#include "libupnpp/control/description.hxx"  // for UPnPDeviceDesc, etc
#include "libupnpp/control/discovery.hxx"  // for UPnPDeviceDirectory, etc
#include "libupnpp/control/renderingcontrol.hxx"  // for RenderingControl, etc
#include "libupnpp/log.hxx"             // for LOGERR, LOGINF

using namespace std;
using namespace std::placeholders;

namespace UPnPClient {

const string 
MediaRenderer::DType("urn:schemas-upnp-org:device:MediaRenderer:1");

// We don't include a version in comparisons, as we are satisfied with
// version 1
bool MediaRenderer::isMRDevice(const string& st)
{
    const string::size_type sz(DType.size()-2);
    return !DType.compare(0, sz, st, 0, sz);
}

// Look at all service descriptions and store parent devices for
// either UPnP RenderingControl or OpenHome Product. Some entries will
// be set multiple times, which does not matter
static bool MDAccum(unordered_map<string, UPnPDeviceDesc>* out,
                    const string& friendlyName,
                    const UPnPDeviceDesc& device, 
                    const UPnPServiceDesc& service)
{
    //LOGDEB("MDAccum: friendlyname: " << friendlyName << 
    //    " dev friendlyName " << device.friendlyName << endl);
    if (
        (RenderingControl::isRDCService(service.serviceType) ||
         OHProduct::isOHPrService(service.serviceType))
        &&
        (friendlyName.empty() || !friendlyName.compare(device.friendlyName))) {
        //LOGDEB("MDAccum setting " << device.UDN << endl);
        (*out)[device.UDN] = device;
    }
    return true;
}

bool MediaRenderer::getDeviceDescs(vector<UPnPDeviceDesc>& devices, 
                                   const string& friendlyName)
{
    unordered_map<string, UPnPDeviceDesc> mydevs;

    UPnPDeviceDirectory::Visitor visitor = bind(MDAccum, &mydevs, friendlyName,
                                                _1, _2);
    UPnPDeviceDirectory::getTheDir()->traverse(visitor);
    for (auto it = mydevs.begin(); it != mydevs.end(); it++)
        devices.push_back(it->second);
    return !devices.empty();
}

MediaRenderer::MediaRenderer(const UPnPDeviceDesc& desc)
    : Device(desc)
{
}

bool MediaRenderer::hasOpenHome()
{
    return ohpr() ? true : false;
}


RDCH MediaRenderer::rdc() 
{
    auto rdcl = m_rdc.lock();
    if (rdcl)
        return rdcl;
    for (auto it = m_desc.services.begin(); it != m_desc.services.end(); it++) {
        if (RenderingControl::isRDCService(it->serviceType)) {
            rdcl = RDCH(new RenderingControl(m_desc, *it));
            break;
        }
    }
    if (!rdcl)
        LOGDEB("MediaRenderer: RenderingControl service not found" << endl);
    m_rdc = rdcl;
    return rdcl;
}

AVTH MediaRenderer::avt() 
{
    auto avtl = m_avt.lock();
    if (avtl)
        return avtl;
    for (auto it = m_desc.services.begin(); it != m_desc.services.end(); it++) {
        if (AVTransport::isAVTService(it->serviceType)) {
            avtl = AVTH(new AVTransport(m_desc, *it));
            break;
        }
    }
    if (!avtl)
        LOGDEB("MediaRenderer: AVTransport service not found" << endl);
    m_avt = avtl;
    return avtl;
}

OHPRH MediaRenderer::ohpr() 
{
    auto ohprl = m_ohpr.lock();
    if (ohprl)
        return ohprl;
    for (auto it = m_desc.services.begin(); it != m_desc.services.end(); it++) {
        if (OHProduct::isOHPrService(it->serviceType)) {
            ohprl = OHPRH(new OHProduct(m_desc, *it));
            break;
        }
    }
    if (!ohprl)
        LOGDEB("MediaRenderer: OHProduct service not found" << endl);
    m_ohpr = ohprl;
    return ohprl;
}

OHPLH MediaRenderer::ohpl() 
{
    auto ohpll = m_ohpl.lock();
    if (ohpll)
        return ohpll;
    for (auto it = m_desc.services.begin(); it != m_desc.services.end(); it++) {
        if (OHPlaylist::isOHPlService(it->serviceType)) {
            ohpll = OHPLH(new OHPlaylist(m_desc, *it));
            break;
        }
    }
    if (!ohpll)
        LOGDEB("MediaRenderer: OHPlaylist service not found" << endl);
    m_ohpl = ohpll;
    return ohpll;
}

OHTMH MediaRenderer::ohtm() 
{
    auto ohtml = m_ohtm.lock();
    if (ohtml)
        return ohtml;
    for (auto it = m_desc.services.begin(); it != m_desc.services.end(); it++) {
        if (OHTime::isOHTMService(it->serviceType)) {
            ohtml = OHTMH(new OHTime(m_desc, *it));
            break;
        }
    }
    if (!ohtml)
        LOGDEB("MediaRenderer: OHTime service not found" << endl);
    m_ohtm = ohtml;
    return ohtml;
}

OHVLH MediaRenderer::ohvl() 
{
    auto ohvll = m_ohvl.lock();
    if (ohvll)
        return ohvll;
    for (auto it = m_desc.services.begin(); it != m_desc.services.end(); it++) {
        if (OHVolume::isOHVLService(it->serviceType)) {
            ohvll = OHVLH(new OHVolume(m_desc, *it));
            break;
        }
    }
    if (!ohvll)
        LOGDEB("MediaRenderer: OHVolume service not found" << endl);
    m_ohvl = ohvll;
    return ohvll;
}

}
