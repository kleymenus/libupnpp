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
#include "libupnpp/control/service.hxx"

#include <upnp/upnp.h>                  // for Upnp_Event, UPNP_E_SUCCESS, etc
#include <upnp/upnptools.h>             // for UpnpGetErrorMessage

#include <functional>                   // for function
#include <string>                       // for string, char_traits, etc
#include <unordered_map>                // for unordered_map, operator!=, etc
#include <utility>                      // for pair

#include "libupnpp/control/description.hxx"  // for UPnPDeviceDesc, etc
#include "libupnpp/ixmlwrap.hxx"
#include "libupnpp/log.hxx"             // for LOGDEB1, LOGINF, LOGERR, etc
#include "libupnpp/ptmutex.hxx"         // for PTMutexLocker, PTMutexInit
#include "libupnpp/upnpp_p.hxx"         // for caturl
#include "libupnpp/upnpplib.hxx"        // for LibUPnP

using namespace std;
using namespace std::placeholders;

namespace UPnPClient {

// A small helper class for the functions which perform
// UpnpSendAction calls: get rid of IXML docs when done.
class IxmlCleaner {
public:
    IXML_Document **rqpp, **rspp;
    IxmlCleaner(IXML_Document** _rqpp, IXML_Document **_rspp)
        : rqpp(_rqpp), rspp(_rspp) {}
    ~IxmlCleaner()
     {
         if (*rqpp) ixmlDocument_free(*rqpp);
         if (*rspp) ixmlDocument_free(*rspp);
     }
};

Service::Service(const UPnPDeviceDesc& device,
                 const UPnPServiceDesc& service, bool doSubscribe)
    : m_reporter(0), 
      m_actionURL(caturl(device.URLBase, service.controlURL)),
      m_eventURL(caturl(device.URLBase, service.eventSubURL)),
      m_serviceType(service.serviceType),
      m_deviceId(device.UDN),
      m_friendlyName(device.friendlyName),
      m_manufacturer(device.manufacturer),
      m_modelName(device.modelName)
{ 
    // Only does anything the first time
    initEvents();
    if (doSubscribe)
        subscribe();
}

Service::~Service()
{
    LOGDEB1("Service::~Service: unregister " << m_SID << endl);
    if (m_SID[0]) {
        unSubscribe();
        o_calls.erase(m_SID);
    }
}

int Service::runAction(const SoapEncodeInput& args, SoapDecodeOutput& data)
{
    LibUPnP* lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        LOGINF("Service::runAction: no lib" << endl);
        return UPNP_E_OUTOF_MEMORY;
    }
    UpnpClient_Handle hdl = lib->getclh();

    IXML_Document *request(0);
    IXML_Document *response(0);
    IxmlCleaner cleaner(&request, &response);

    if ((request = buildSoapBody(args, false)) == 0) {
        LOGINF("Service::runAction: buildSoapBody failed" << endl);
        return  UPNP_E_OUTOF_MEMORY;
    }

    LOGDEB1("Service::runAction: rqst: [" << 
            ixmlwPrintDoc(request) << "]" << endl);

    int ret = UpnpSendAction(hdl, m_actionURL.c_str(), m_serviceType.c_str(),
                             0 /*devUDN*/, request, &response);

    if (ret != UPNP_E_SUCCESS) {
        LOGINF("Service::runAction: UpnpSendAction failed: " << ret << 
               " : " << UpnpGetErrorMessage(ret) << " for " << 
               ixmlwPrintDoc(request) << endl);
        return ret;
    }
    LOGDEB1("Service::runAction: rslt: [" << 
            ixmlwPrintDoc(response) << "]" << endl);

    if (!decodeSoapBody(args.name.c_str(), response, &data)) {
        LOGERR("Service::runAction: Could not decode response: " <<
               ixmlwPrintDoc(response) << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return UPNP_E_SUCCESS;
}


static PTMutexInit cblock;
int Service::srvCB(Upnp_EventType et, void* vevp, void*)
{
    PTMutexLocker lock(cblock);

    LOGDEB1("Service:srvCB: " << LibUPnP::evTypeAsString(et) << endl);

    switch (et) {
    case UPNP_EVENT_RENEWAL_COMPLETE:
    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
    case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
    case UPNP_EVENT_AUTORENEWAL_FAILED:
    {
        const char *ff = (const char *)vevp;
        LOGDEB1("Service:srvCB: subs event: " << ff << endl);
        break;
    }

    case UPNP_EVENT_RECEIVED:
    {
        struct Upnp_Event *evp = (struct Upnp_Event *)vevp;
        LOGDEB1("Service:srvCB: var change event: Sid: " <<
                evp->Sid << " EventKey " << evp->EventKey << 
                " changed " << ixmlwPrintDoc(evp->ChangedVariables) << endl);
        
        unordered_map<string, string> props;
        if (!decodePropertySet(evp->ChangedVariables, props)) {
            LOGERR("Service::srvCB: could not decode EVENT propertyset" <<endl);
            return UPNP_E_BAD_RESPONSE;
        }
        //for (auto& entry: props) {
        //LOGDEB("srvCB: " << entry.first << " -> " << entry.second << endl);
        //}

        std::unordered_map<std::string, evtCBFunc>::iterator it = 
            o_calls.find(evp->Sid);
        if (it!= o_calls.end()) {
            (it->second)(props);
        } else {
            LOGINF("Service::srvCB: no callback found for sid " << evp->Sid << 
                   endl);
        }
        break;
    }

    default:
        // Ignore other events for now
        LOGDEB("Service:srvCB: unprocessed evt type: [" << 
               LibUPnP::evTypeAsString(et) << "]"  << endl);
        break;
    }

    return UPNP_E_SUCCESS;
}

// This is called once per process.
bool Service::initEvents()
{
    LOGDEB1("Service::initEvents" << endl);

    PTMutexLocker lock(cblock);
    static bool eventinit(false);
    if (eventinit)
        return true;
    eventinit = true;

    LibUPnP *lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        LOGERR("Service::initEvents: Can't get lib" << endl);
        return false;
    }
    lib->registerHandler(UPNP_EVENT_RENEWAL_COMPLETE, srvCB, 0);
    lib->registerHandler(UPNP_EVENT_SUBSCRIBE_COMPLETE, srvCB, 0);
    lib->registerHandler(UPNP_EVENT_UNSUBSCRIBE_COMPLETE, srvCB, 0);
    lib->registerHandler(UPNP_EVENT_AUTORENEWAL_FAILED, srvCB, 0);
    lib->registerHandler(UPNP_EVENT_RECEIVED, srvCB, 0);
    return true;
}

//void Service::evtCallback(
//    const std::unordered_map<std::string, std::string>*)
//{
//    LOGDEB("Service::evtCallback!! service: " << m_serviceType << endl);
//}

bool Service::subscribe()
{
    LOGDEB1("Service::subscribe" << endl);
    LibUPnP* lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        LOGINF("Service::subscribe: no lib" << endl);
        return UPNP_E_OUTOF_MEMORY;
    }
    int timeout = 1800;
    int ret = UpnpSubscribe(lib->getclh(), m_eventURL.c_str(),
                            &timeout, m_SID);
    if (ret != UPNP_E_SUCCESS) {
        LOGERR("Service:subscribe: failed: " << ret << " : " <<
               UpnpGetErrorMessage(ret) << endl);
        return false;
    } 
    LOGDEB1("Service::subscribe: sid: " << m_SID << endl);
    return true;
}

bool Service::unSubscribe()
{
    LOGDEB1("Service::unSubscribe" << endl);
    LibUPnP* lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        LOGINF("Service::unSubscribe: no lib" << endl);
        return UPNP_E_OUTOF_MEMORY;
    }
    int ret = UpnpUnSubscribe(lib->getclh(), m_SID);
    if (ret != UPNP_E_SUCCESS) {
        LOGERR("Service:unSubscribe: failed: " << ret << " : " <<
               UpnpGetErrorMessage(ret) << endl);
        return false;
    } 
    return true;
}

void Service::registerCallback(evtCBFunc c)
{
    PTMutexLocker lock(cblock);
    LOGDEB1("Service::registerCallback: " << m_SID << endl);
    o_calls[m_SID] = c;
}

std::unordered_map<std::string, evtCBFunc> Service::o_calls;

}
