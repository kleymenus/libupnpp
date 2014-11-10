/* Copyright (C) 2014 J.F.Dockes
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
#ifndef _DEVICE_H_X_INCLUDED_
#define _DEVICE_H_X_INCLUDED_

#include <pthread.h>                    // for pthread_cond_t
#include <upnp/upnp.h>                  // for Upnp_EventType, etc

#include <functional>                   // for function
#include <string>                       // for string
#include <unordered_map>                // for unordered_map, etc
#include <vector>                       // for vector

#include "libupnpp/ptmutex.hxx"         // for PTMutexInit
#include "libupnpp/soaphelp.hxx"        // for SoapOutgoing, SoapIncoming

namespace UPnPP { class LibUPnP; }
namespace UPnPProvider { class UpnpService; }

namespace UPnPProvider {

typedef std::function<int (const UPnPP::SoapIncoming&, UPnPP::SoapOutgoing&)> soapfun;

// Definition of a virtual directory entry: data and mime type
struct VDirContent {
    VDirContent(const std::string& ct, const std::string& mt)
        : content(ct), mimetype(mt) {}
    std::string content;
    std::string mimetype;
};

/** Define a virtual interface to link libupnp operations to a device 
 * implementation 
 */
class UpnpDevice {
public:
    /** Construct device object
     * @param deviceId uuid for device: "uuid:UUIDvalue"
     * @param files list of name/content pairs to be added to the
     *   virtual directory root. The file names must match the SCDPURL
     *   values for the services in the description.xml document. Note that 
     *   this will have to be changed if we ever want to really
     *   support multiple devices (will need to use subdirectories or
     *   find another way to avoid name conflicts).
     */
    UpnpDevice(const std::string& deviceId, 
               const std::unordered_map<std::string, VDirContent>& files);
    ~UpnpDevice();

    void addService(UpnpService *, const std::string& serviceId);

    /**
     * Add mapping from service+action-name to handler function.
     */
    void addActionMapping(const UpnpService*, 
                          const std::string& actName, soapfun);

    /** 
     * Main routine. To be called by main() when done with initialization. 
     *
     * This loop mostly polls getEventData and generates an UPnP event if
     * there is anything to broadcast. The UPnP action calls happen in
     * other threads with which we synchronize, currently using a global lock.
     */
    void eventloop();

    /** 
     * To be called from a service action callback to wake up the
     * event loop early if something needs to be broadcast without
     * waiting for the normal delay.
     *
     * Will only do something if the previous event is not too recent.
     */
    void loopWakeup(); // To trigger an early event

    /**
     * To be called to get the event loop to return
     */
    void shouldExit();

    /** Check status */
    bool ok() {return m_lib != 0;}

private:
    const std::string& serviceType(const std::string& serviceId);
            
    UPnPP::LibUPnP *m_lib;
    std::string m_deviceId;
    // We keep the services in a map for easy access from id and in a
    // vector for ordered walking while fetching status. Order is
    // determine by addService() call sequence.
    std::unordered_map<std::string, UpnpService*> m_servicemap;
    std::vector<std::string> m_serviceids;
    std::unordered_map<std::string, soapfun> m_calls;
    std::unordered_map<std::string, UpnpService*>::const_iterator findService(const std::string& serviceid);

    bool m_needExit;
    /* My device handle */
    UpnpDevice_Handle m_dvh;

    /* Lock for device operations. Held during a service callback 
       Must not be held when using m_dvh to call into libupnp */
    UPnPP::PTMutexInit m_lock;

    pthread_cond_t m_evloopcond;
    UPnPP::PTMutexInit m_evlooplock;

    /* Gets called when something needs doing */
    int callBack(Upnp_EventType et, void* evp);

    /** 
     * Generate event.
     *
     * Called by the device event loop, which polls the services.
     * Use loopwakeup() to expedite things.
     */
    void notifyEvent(const std::string& serviceId,
                     const std::vector<std::string>& names, 
                     const std::vector<std::string>& values);


    /** Static array of devices for dispatching */
    static std::unordered_map<std::string, UpnpDevice *> o_devices;

    /* Static callback for libupnp. This looks up the appropriate
     * device using the device ID (UDN), the calls its callback
     * method */
    static int sCallBack(Upnp_EventType et, void* evp, void*);
};

/**
 * Upnp service implementation class. This does not do much useful beyond
 * encapsulating the service actions and event callback. In most cases, the
 * services will need full access to the device state anyway.
 */
class UpnpService {
public:
    UpnpService(const std::string& stp,const std::string& sid, UpnpDevice *dev) 
        : m_serviceType(stp), m_serviceId(sid)
        {
            dev->addService(this, sid);
        }
    virtual ~UpnpService() {}

    /** 
     * Poll to retrieve evented data changed since last call.
     *
     * To be implemented by the derived class.
     * Called by the library when a control point subscribes, to
     * retrieve eventable data. 
     * Return name/value pairs for changed variables in the data arrays.
     */
    virtual bool getEventData(bool all, std::vector<std::string>& names, 
                              std::vector<std::string>& values) 
        {
            return true;
        }

    virtual const std::string& getServiceType() const
        {
            return m_serviceType;
        }
    virtual const std::string& getServiceId() const
        {
            return m_serviceId;
        }

protected:
    const std::string m_serviceType;
    const std::string m_serviceId;
};

} // End namespace UPnPProvider

#endif /* _DEVICE_H_X_INCLUDED_ */
