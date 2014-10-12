/* Copyright (C) 2013 J.F.Dockes
 *	 This program is free software; you can redistribute it and/or modify
 *	 it under the terms of the GNU General Public License as published by
 *	 the Free Software Foundation; either version 2 of the License, or
 *	 (at your option) any later version.
 *
 *	 This program is distributed in the hope that it will be useful,
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	 GNU General Public License for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the
 *	 Free Software Foundation, Inc.,
 *	 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "config.h"

#include "libupnpp/control/cdirectory.hxx"

#include <sys/types.h>
#include <regex.h>

#include <upnp/upnp.h>                  // for UPNP_E_SUCCESS, etc
#include <upnp/upnptools.h>             // for UpnpGetErrorMessage

#include <functional>                   // for _Bind, bind, _1, _2
#include <iostream>                     // for operator<<, basic_ostream, etc
#include <set>                          // for set
#include <string>                       // for string, operator<<, etc
#include <vector>                       // for vector

#include "libupnpp/control/cdircontent.hxx"  // for UPnPDirContent
#include "libupnpp/control/description.hxx"  // for UPnPDeviceDesc, etc
#include "libupnpp/control/discovery.hxx"  // for UPnPDeviceDirectory, etc
#include "libupnpp/log.hxx"             // for LOGDEB, LOGINF, LOGERR
#include "libupnpp/soaphelp.hxx"        // for SoapEncodeInput, SoapArgs, etc
#include "libupnpp/upnpp_p.hxx"         // for csvToStrings

using namespace std;
using namespace std::placeholders;

namespace UPnPClient {

// The service type string for Content Directories:
const string ContentDirectory::SType("urn:schemas-upnp-org:service:ContentDirectory:1");

class SimpleRegexp {
public:
    SimpleRegexp(const string& exp, int flags) : m_ok(false) {
        if (regcomp(&m_expr, exp.c_str(), flags) == 0) {
            m_ok = true;
        }
    }
    ~SimpleRegexp() {
        regfree(&m_expr);
    }
    bool simpleMatch(const string& val) const {
        if (!ok())
            return false;
        if (regexec(&m_expr, val.c_str(), 0, 0, 0) == 0) {
            return true;
        } else {
            return false;
        }
    }
    bool operator() (const string& val) const {
        return simpleMatch(val);
    }

    bool ok() const {return m_ok;}
private:
    bool m_ok;
    regex_t m_expr;
};

/*
  manufacturer: Bubblesoft model BubbleUPnP Media Server
  manufacturer: Justin Maggard model Windows Media Connect compatible (MiniDLNA)
  manufacturer: minimserver.com model MinimServer
  manufacturer: PacketVideo model TwonkyMedia Server
  manufacturer: ? model MediaTomb
*/
static const SimpleRegexp bubble_rx("bubble", REG_ICASE|REG_NOSUB);
static const SimpleRegexp mediatomb_rx("mediatomb", REG_ICASE|REG_NOSUB);
static const SimpleRegexp minidlna_rx("minidlna", REG_ICASE|REG_NOSUB);
static const SimpleRegexp minim_rx("minim", REG_ICASE|REG_NOSUB);
static const SimpleRegexp twonky_rx("twonky", REG_ICASE|REG_NOSUB);

ContentDirectory::ContentDirectory(const UPnPDeviceDesc& device,
                                   const UPnPServiceDesc& service)
    : Service(device, service), m_rdreqcnt(200), m_serviceKind(CDSKIND_UNKNOWN)
{
    LOGERR("ContentDirectory::ContentDirectory: manufacturer: " << 
           m_manufacturer << " model " << m_modelName << endl);

    if (bubble_rx(m_modelName)) {
        m_serviceKind = CDSKIND_BUBBLE;
        LOGDEB1("ContentDirectory::ContentDirectory: BUBBLE" << endl);
    } else if (mediatomb_rx(m_modelName)) {
        // Readdir by 200 entries is good for most, but MediaTomb likes
        // them really big. Actually 1000 is better but I don't dare
        m_rdreqcnt = 500;
        m_serviceKind = CDSKIND_MEDIATOMB;
        LOGDEB1("ContentDirectory::ContentDirectory: MEDIATOMB" << endl);
    } else if (minidlna_rx(m_modelName)) {
        m_serviceKind = CDSKIND_MINIDLNA;
        LOGDEB1("ContentDirectory::ContentDirectory: MINIDLNA" << endl);
    } else if (minim_rx(m_modelName)) {
        m_serviceKind = CDSKIND_MINIM;
        LOGDEB1("ContentDirectory::ContentDirectory: MINIM" << endl);
    } else if (twonky_rx(m_modelName)) {
        m_serviceKind = CDSKIND_TWONKY;
        LOGDEB1("ContentDirectory::ContentDirectory: TWONKY" << endl);
    } 
    registerCallback();
}

// We don't include a version in comparisons, as we are satisfied with
// version 1
bool ContentDirectory::isCDService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}

static bool DSAccum(vector<CDSH>* out,
                    const UPnPDeviceDesc& device, 
                    const UPnPServiceDesc& service)
{
    if (ContentDirectory::isCDService(service.serviceType)) {
        out->push_back(CDSH(new ContentDirectory(device, service)));
    }
    return true;
}

bool ContentDirectory::getServices(vector<CDSH>& vds)
{
    //LOGDEB("UPnPDeviceDirectory::getDirServices" << endl);
    UPnPDeviceDirectory::Visitor visitor = bind(DSAccum, &vds, _1, _2);
    UPnPDeviceDirectory::getTheDir()->traverse(visitor);
    return !vds.empty();
}

// Get server by friendly name. 
bool ContentDirectory::getServerByName(const string& fname, CDSH& server)
{
    UPnPDeviceDesc ddesc;
    bool found = UPnPDeviceDirectory::getTheDir()->getDevByFName(fname, ddesc);
    if (!found)
        return false;

    found = false;
    for (auto it = ddesc.services.begin(); it != ddesc.services.end(); it++) {
        if (isCDService(it->serviceType)) {
            server = CDSH(new ContentDirectory(ddesc, *it));
            found = true;
            break;
        }
    }
    return found;
}

#if 0
static int asyncReaddirCB(Upnp_EventType et, void *vev, void *cookie)
{
    LOGDEB("asyncReaddirCB: " << LibUPnP::evTypeAsString(et) << endl);
    struct Upnp_Action_Complete *act = (struct Upnp_Action_Complete*)vev;

    LOGDEB("asyncReaddirCB: errcode " << act->ErrCode << 
           " cturl " <<  UpnpString_get_String(act->CtrlUrl) << 
           " actionrequest " << endl << 
           ixmlwPrintDoc(act->ActionRequest) << endl <<
           " actionresult " << ixmlwPrintDoc(act->ActionResult) << endl);
    return 0;
}
    int ret = 
        UpnpSendActionAsync(hdl, m_actionURL.c_str(), m_serviceType.c_str(),
        0 /*devUDN*/, request, asyncReaddirCB, 0);
    sleep(10);
    return -1;
#endif

void ContentDirectory::evtCallback(const unordered_map<string, string>&)
{
}

void ContentDirectory::registerCallback()
{
    LOGDEB("ContentDirectory::registerCallback"<< endl);
    Service::registerCallback(bind(&ContentDirectory::evtCallback, 
                                   this, _1));
}

int ContentDirectory::readDirSlice(const string& objectId, int offset,
                                          int count, UPnPDirContent& dirbuf,
                                          int *didreadp, int *totalp)
{
    LOGDEB("CDService::readDirSlice: objId [" << objectId << "] offset " << 
           offset << " count " << count << endl);

    // Create request
    // Some devices require an empty SortCriteria, else bad params
    SoapData args(m_serviceType, "Browse");
    args("ObjectID", objectId)
        ("BrowseFlag", "BrowseDirectChildren")
        ("Filter", "*")
        ("SortCriteria", "")
        ("StartingIndex", SoapHelp::i2s(offset))
        ("RequestedCount", SoapHelp::i2s(count));

    SoapArgs data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    int didread;
    string tbuf;
    if (!data.getInt("NumberReturned", &didread) ||
        !data.getInt("TotalMatches", totalp) ||
        !data.getString("Result", &tbuf)) {
        LOGERR("CDService::readDir: missing elts in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }

    if (didread <= 0) {
        LOGINF("CDService::readDir: got -1 or 0 entries" << endl);
        return UPNP_E_BAD_RESPONSE;
    }

#if 0
    cerr << "CDService::readDirSlice: count " << count <<
        " offset " << offset <<
        " total " << *totalp << endl;
    cerr << " result " << tbuf << endl;
#endif

    dirbuf.parse(tbuf);
    *didreadp = didread;

    return UPNP_E_SUCCESS;
}

int ContentDirectory::readDir(const string& objectId,
                                     UPnPDirContent& dirbuf)
{
    LOGDEB("CDService::readDir: url [" << m_actionURL << "] type [" <<
           m_serviceType << "] udn [" << m_deviceId << "] objId [" <<
           objectId << endl);

    int offset = 0;
    int total = 1000;// Updated on first read.

    while (offset < total) {
        int count;
        int error = readDirSlice(objectId, offset, m_rdreqcnt, dirbuf,
                                 &count, &total);
        if (error != UPNP_E_SUCCESS)
            return error;

        offset += count;
    }

    return UPNP_E_SUCCESS;
}

int ContentDirectory::search(const string& objectId,
                                    const string& ss,
                                    UPnPDirContent& dirbuf)
{
    LOGDEB("CDService::search: url [" << m_actionURL << "] type [" << 
           m_serviceType << "] udn [" << m_deviceId << "] objid [" << 
           objectId <<  "] search [" << ss << "]" << endl);

    int offset = 0;
    int total = 1000;// Updated on first read.

    while (offset < total) {
        // Create request
        SoapData args(m_serviceType, "Search");
        args("ContainerID", objectId)
            ("SearchCriteria", ss)
            ("Filter", "*")
            ("SortCriteria", "")
            ("StartingIndex", SoapHelp::i2s(offset))
            ("RequestedCount", "10"); 

        SoapArgs data;
        int ret = runAction(args, data);

        if (ret != UPNP_E_SUCCESS) {
            LOGINF("CDService::search: UpnpSendAction failed: " <<
                   UpnpGetErrorMessage(ret) << endl);
            return ret;
        }

        int count = -1;
        string tbuf;
        if (!data.getInt("NumberReturned", &count) ||
            !data.getInt("TotalMatches", &total) ||
            !data.getString("Result", &tbuf)) {
            LOGERR("CDService::search: missing elts in response" << endl);
            return UPNP_E_BAD_RESPONSE;
        }
        if (count <=  0) {
            LOGINF("CDService::search: got -1 or 0 entries" << endl);
            return count < 0 ? UPNP_E_BAD_RESPONSE : UPNP_E_SUCCESS;
        }
        offset += count;

        dirbuf.parse(tbuf);
    }

    return UPNP_E_SUCCESS;
}

int ContentDirectory::getSearchCapabilities(set<string>& result)
{
    LOGDEB("CDService::getSearchCapabilities:" << endl);

    SoapData args(m_serviceType, "GetSearchCapabilities");
    SoapArgs data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        LOGINF("CDService::getSearchCapa: UpnpSendAction failed: " << 
               UpnpGetErrorMessage(ret) << endl);
        return ret;
    }
    string tbuf;
    if (!data.getString("SearchCaps", &tbuf)) {
        LOGERR("CDService::getSearchCaps: missing Result in response" << endl);
        cerr << tbuf << endl;
        return UPNP_E_BAD_RESPONSE;
    }

    result.clear();
    if (!tbuf.compare("*")) {
        result.insert(result.end(), "*");
    } else if (!tbuf.empty()) {
        if (!csvToStrings(tbuf, result)) {
            return UPNP_E_BAD_RESPONSE;
        }
    }

    return UPNP_E_SUCCESS;
}

int ContentDirectory::getMetadata(const string& objectId,
                                         UPnPDirContent& dirbuf)
{
    LOGDEB("CDService::getMetadata: url [" << m_actionURL << "] type [" <<
           m_serviceType << "] udn [" << m_deviceId << "] objId [" <<
           objectId << "]" << endl);

    SoapData args(m_serviceType, "Browse");
    SoapArgs data;
    args("ObjectID", objectId)
        ("BrowseFlag", "BrowseMetadata")
        ("Filter", "*")
        ("SortCriteria", "")
        ("StartingIndex", "0")
        ("RequestedCount", "1");
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        LOGINF("CDService::getmetadata: UpnpSendAction failed: " << 
               UpnpGetErrorMessage(ret) << endl);
        return ret;
    }
    string tbuf;
    if (!data.getString("Result", &tbuf)) {
        LOGERR("CDService::getmetadata: missing Result in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }

    if (dirbuf.parse(tbuf))
        return UPNP_E_SUCCESS;
    else
        return UPNP_E_BAD_RESPONSE;
}

} // namespace UPnPClient
