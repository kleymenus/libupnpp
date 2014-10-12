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

#include "libupnpp/control/renderingcontrol.hxx"

#include <stdlib.h>                     // for atoi
#include <upnp/upnp.h>                  // for UPNP_E_BAD_RESPONSE, etc

#include <functional>                   // for _Bind, bind, _1
#include <ostream>                      // for basic_ostream, endl, etc
#include <string>                       // for string, operator<<, etc
#include <utility>                      // for pair

#include "libupnpp/control/description.hxx"
#include "libupnpp/control/avlastchg.hxx"  // for decodeAVLastChange
#include "libupnpp/control/service.hxx"  // for VarEventReporter, Service
#include "libupnpp/log.hxx"             // for LOGERR, LOGDEB1, LOGINF
#include "libupnpp/soaphelp.hxx"        // for SoapEncodeInput, etc
#include "libupnpp/upnpp_p.hxx"         // for stringToBool

using namespace std;
using namespace std::placeholders;

namespace UPnPClient {

const string 
RenderingControl::SType("urn:schemas-upnp-org:service:RenderingControl:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool RenderingControl::isRDCService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}

RenderingControl::RenderingControl(const UPnPDeviceDesc& device,
                                   const UPnPServiceDesc& service)
    : Service(device, service), m_volmin(0), m_volmax(100), m_volstep(1)
{
    UPnPServiceDesc::Parsed sdesc;
    if (service.fetchAndParseDesc(device.URLBase, sdesc)) {
        auto it = sdesc.stateTable.find("Volume");
        if (it != sdesc.stateTable.end() && it->second.hasValueRange) {
            setVolParams(it->second.minimum, it->second.maximum,
                         it->second.step);
        }
    }
    registerCallback();
}

int RenderingControl::devVolTo0100(int dev_vol)
{
    int volume;
    if (dev_vol < m_volmin)
        dev_vol = m_volmin;
    if (dev_vol > m_volmax)
        dev_vol = m_volmax;
    if (m_volmin != 0 || m_volmax != 100) {
        float fact = float(m_volmax - m_volmin) / 100.0;
        if (fact <= 0.0) // ??
            fact = 1.0;
        volume = int((dev_vol - m_volmin) / fact);
    } else {
        volume = dev_vol;
    }
    return volume;
}

void RenderingControl::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("RenderingControl::evtCallback: m_reporter " << m_reporter << endl);
    for (auto it = props.begin(); it != props.end(); it++) {
        if (!it->first.compare("LastChange")) {
            std::unordered_map<std::string, std::string> props1;
            if (!decodeAVLastChange(it->second, props1)) {
                LOGERR("RenderingControl::evtCallback: bad LastChange value: "
                       << it->second << endl);
                return;
            }
            for (auto it1 = props1.begin(); it1 != props1.end(); it1++) {
                LOGDEB1("    " << it1->first << " -> " << 
                        it1->second << endl);
                if (!it1->first.compare("Volume")) {
                    int vol = devVolTo0100(atoi(it1->second.c_str()));
                    if (m_reporter) {
                        m_reporter->changed(it1->first.c_str(), vol);
                    }
                } else if (!it1->first.compare("Mute")) {
                    bool mute;
                    if (m_reporter && stringToBool(it1->second, &mute))
                        m_reporter->changed(it1->first.c_str(), mute);
                }
            }
        } else {
            LOGINF("RenderingControl:event: var not lastchange: "
                   << it->first << " -> " << it->second << endl;);
        }
    }
}

void RenderingControl::registerCallback()
{
    Service::registerCallback(bind(&RenderingControl::evtCallback, this, _1));
}

int RenderingControl::setVolume(int ivol, const string& channel)
{
    // Input is always 0-100. Translate to actual range
    if (ivol < 0)
        ivol = 0;
    if (ivol > 100)
        ivol = 100;
    int volume = ivol;
    if (m_volmin != 0 || m_volmax != 100) {
        float fact = float(m_volmax - m_volmin) / 100.0;
        volume = m_volmin + int(float(ivol) * fact);
    }
    //LOGINF("RenderingControl::setVolume: ivol " << ivol << 
    //      " m_volmin " << m_volmin << " m_volmax " << m_volmax <<
    //      " m_volstep " << m_volstep << " volume " << volume << endl);

    SoapEncodeInput args(m_serviceType, "SetVolume");
    args("InstanceID", "0")("Channel", channel)
        ("DesiredVolume", SoapHelp::i2s(volume));
    SoapDecodeOutput data;
    return runAction(args, data);
}

int RenderingControl::getVolume(const string& channel)
{
    SoapEncodeInput args(m_serviceType, "GetVolume");
    args("InstanceID", "0")("Channel", channel);
    SoapDecodeOutput data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    int dev_volume;
    if (!data.getInt("CurrentVolume", &dev_volume)) {
        LOGERR("RenderingControl:getVolume: missing CurrentVolume in response" 
        << endl);
        return UPNP_E_BAD_RESPONSE;
    }

    // Output is always 0-100. Translate from device range
    return devVolTo0100(dev_volume);
}

int RenderingControl::setMute(bool mute, const string& channel)
{
    SoapEncodeInput args(m_serviceType, "SetMute");
    args("InstanceID", "0")("Channel", channel)
        ("DesiredMute", SoapHelp::i2s(mute?1:0));
    SoapDecodeOutput data;
    return runAction(args, data);
}

bool RenderingControl::getMute(const string& channel)
{
    SoapEncodeInput args(m_serviceType, "GetMute");
    args("InstanceID", "0")("Channel", channel);
    SoapDecodeOutput data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    bool mute;
    if (!data.getBool("CurrentMute", &mute)) {
        LOGERR("RenderingControl:getMute: missing CurrentMute in response" 
        << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return mute;
}

} // End namespace UPnPClient
