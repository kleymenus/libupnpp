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
#ifndef _SERVICE_H_X_INCLUDED_
#define _SERVICE_H_X_INCLUDED_

#include <upnp/upnp.h>                  // for UPNP_E_BAD_RESPONSE, etc

#include <functional>                   // for function
#include <iostream>                     // for basic_ostream, operator<<, etc
#include <string>                       // for string, operator<<, etc
#include <unordered_map>                // for unordered_map
#include <vector>                       // for vector

#include "libupnpp/control/cdircontent.hxx"  // for UPnPDirObject
#include "libupnpp/log.hxx"             // for LOGERR
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, etc

namespace UPnPClient { class UPnPDeviceDesc; }
namespace UPnPClient { class UPnPServiceDesc; }

using namespace UPnPP;

namespace UPnPClient {

class Service;

/** To be implemented by upper-level client code for event
 * reporting. Runs in an event thread. This could for example be
 * implemented by a Qt Object to generate events for the GUI.
 */
class VarEventReporter {
public:
    // Using char * to avoid any issue with strings and concurrency
    virtual void changed(const char *nm, int val)  = 0;
    virtual void changed(const char *nm, const char *val) = 0;
    // Used for track metadata (parsed as content directory entry). Not always
    // needed.
    virtual void changed(const char */*nm*/, UPnPDirObject /*meta*/) {};
    // Used by ohplaylist. Not always needed
    virtual void changed(const char */*nm*/, std::vector<int> /*ids*/) {};
};

typedef 
std::function<void (const std::unordered_map<std::string, std::string>&)> 
evtCBFunc;

class Service {
public:
    /** Construct by copying data from device and service objects.
     */
    Service(const UPnPDeviceDesc& device, const UPnPServiceDesc& service); 

    /** An empty one */
    Service() : m_reporter(0) {}

    virtual ~Service();

    /** Retrieve my root device "friendly name". */
    std::string getFriendlyName() const {return m_friendlyName;}

    /** Return my root device id */
    std::string getDeviceId() const {return m_deviceId;}

    virtual int runAction(const SoapOutgoing& args, SoapIncoming& data);

    virtual VarEventReporter *getReporter()
    {
        return m_reporter;
    }

    virtual void installReporter(VarEventReporter* reporter)
    {
        m_reporter = reporter;
    }

    // Can't copy these because this does not make sense for the
    // member function callback.
    Service(Service const&) = delete;
    Service& operator=(Service const&) = delete;

protected:

    /** Registered callbacks for the service objects. The map is
     * indexed by m_SID, the subscription id which was obtained by
     * each object when subscribing to receive the events for its
     * device. The map allows the static function registered with
     * libupnp to call the appropriate object method when it receives
     * an event. */
    static std::unordered_map<std::string, evtCBFunc> o_calls;

    /** Used by a derived class to register its callback method. This
     * creates an entry in the static map, using m_SID, which was
     * obtained by subscribe() during construction 
     */
    void registerCallback(evtCBFunc c);
    void unregisterCallback();

    /** Upper level client code event callbacks */
    VarEventReporter *m_reporter;

    std::string m_actionURL;
    std::string m_eventURL;
    std::string m_serviceType;
    std::string m_deviceId;
    std::string m_friendlyName;
    std::string m_manufacturer;
    std::string m_modelName;
    
    /** Run trivial action where there are neither input parameters
       nor return data (beyond the status) */
    int runTrivialAction(const std::string& actionName) {
        SoapOutgoing args(m_serviceType, actionName);
        SoapIncoming data;
        return runAction(args, data);
    }

    /* Run action where there are no input parameters and a single
       named value is to be retrieved from the result */
    template <class T> int runSimpleGet(const std::string& actnm, 
                                        const std::string& valnm,
                                        T *valuep) {
        SoapOutgoing args(m_serviceType, actnm);
        SoapIncoming data;
        int ret = runAction(args, data);
        if (ret != UPNP_E_SUCCESS) {
            return ret;
        }
        if (!data.get(valnm.c_str(), valuep)) {
            LOGERR("Service::runSimpleAction: " << actnm << 
                   " missing " << valnm << " in response" << std::endl);
            return UPNP_E_BAD_RESPONSE;
        }
        return 0;
    }

    /* Run action with a single input parameter and no return data */
    template <class T> int runSimpleAction(const std::string& actnm, 
                                           const std::string& valnm,
                                           T value) {
        SoapOutgoing args(m_serviceType, actnm);
        args(valnm, SoapHelp::val2s(value));
        SoapIncoming data;
        return runAction(args, data);
    }

private:
    /** Only actually does something on the first call, to register our
     * (static) library callback */
    static bool initEvents();
    /** The static event callback given to libupnp */
    static int srvCB(Upnp_EventType et, void* vevp, void*);
    /* Tell the UPnP device (through libupnp) that we want to receive
       its events. This is called during construction and sets m_SID */
    virtual bool subscribe();
    virtual bool unSubscribe();

    Upnp_SID    m_SID; /* Subscription Id */
};

} // namespace UPnPClient

#endif /* _SERVICE_H_X_INCLUDED_ */
