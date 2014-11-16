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
#include "config.h"

#include "device.hxx"

#include <errno.h>                      // for ETIMEDOUT, errno
#include <sys/time.h>                   // for CLOCK_REALTIME
#include <time.h>                       // for timespec, clock_gettime

#include <iostream>                     // for endl, operator<<, etc
#include <utility>                      // for pair

#include "libupnpp/log.hxx"             // for LOGERR, LOGFAT, LOGDEB, etc
#include "libupnpp/ixmlwrap.hxx"
#include "libupnpp/upnpplib.hxx"        // for LibUPnP
#include "libupnpp/upnpputils.hxx"      // for timespec_addnanos
#include "libupnpp/upnpp_p.hxx"
#include "vdir.hxx"                     // for VirtualDir

using namespace std;
using namespace UPnPP;

namespace UPnPProvider {

class UpnpDevice::Internal {
public:
    Internal() : evloopcond(PTHREAD_COND_INITIALIZER) {}
    /* Generate event: called by the device event loop, which polls
     * the services. */
    void notifyEvent(const std::string& serviceId,
                     const std::vector<std::string>& names, 
                     const std::vector<std::string>& values);
    bool start();

    std::unordered_map<std::string, UpnpService*>::const_iterator 
        findService(const std::string& serviceid);

    /* Per-device callback */
    int callBack(Upnp_EventType et, void* evp);

    UPnPP::LibUPnP *lib;
    std::string deviceId;
    std::string description;

    // We keep the services in a map for easy access from id and in a
    // vector for ordered walking while fetching status. Order is
    // determine by addService() call sequence.
    std::unordered_map<std::string, UpnpService*> servicemap;
    std::vector<std::string> serviceids;
    std::unordered_map<std::string, soapfun> calls;
    bool needExit;
    /* My device handle */
    UpnpDevice_Handle dvh;
    /* Lock for device operations. Held during a service callback 
       Must not be held when using dvh to call into libupnp */
    UPnPP::PTMutexInit devlock;
    pthread_cond_t evloopcond;
    UPnPP::PTMutexInit evlooplock;
};

class UpnpDevice::InternalStatic {
public:
    /* Static callback for libupnp. This looks up the appropriate
     * device using the device ID (UDN), the calls its callback
     * method */
    static int sCallBack(Upnp_EventType et, void* evp, void*);

    /** Static array of devices for dispatching */
    static std::unordered_map<std::string, UpnpDevice *> devices;
    static PTMutexInit devices_lock;
};

unordered_map<std::string, UpnpDevice *> 
UpnpDevice::InternalStatic::devices;
PTMutexInit UpnpDevice::InternalStatic::devices_lock;
UpnpDevice::InternalStatic *UpnpDevice::o;

static bool vectorstoargslists(const vector<string>& names, 
                               const vector<string>& values,
                               vector<string>& qvalues,
                               vector<const char *>& cnames,
                               vector<const char *>& cvalues)
{
    if (names.size() != values.size()) {
        LOGERR("vectorstoargslists: bad sizes" << endl);
        return false;
    }

    cnames.reserve(names.size());
    qvalues.clear();
    qvalues.reserve(values.size());
    cvalues.reserve(values.size());
    for (unsigned int i = 0; i < values.size(); i++) {
        cnames.push_back(names[i].c_str());
        qvalues.push_back(SoapHelp::xmlQuote(values[i]));
        cvalues.push_back(qvalues[i].c_str());
        //LOGDEB("Edata: " << cnames[i] << " -> " << cvalues[i] << endl);
    }
    return true;
}

static const int expiretime = 3600;

UpnpDevice::UpnpDevice(const string& deviceId, 
                       const unordered_map<string, VDirContent>& files)
{
    if (o == 0 && (o = new InternalStatic()) == 0) {
        LOGERR("UpnpDevice::UpnpDevice: out of memory" << endl);
        return;
    }
    if ((m = new Internal()) == 0) {
        LOGERR("UpnpDevice::UpnpDevice: out of memory" << endl);
        return;
    }
    m->deviceId = deviceId;
    m->needExit = false;
    //LOGDEB("UpnpDevice::UpnpDevice(" << m->deviceId << ")" << endl);

    m->lib = LibUPnP::getLibUPnP(true);
    if (!m->lib) {
        LOGFAT(" Can't get LibUPnP" << endl);
        return;
    }
    if (!m->lib->ok()) {
        LOGFAT("Lib init failed: " <<
               m->lib->errAsString("main", m->lib->getInitError()) << endl);
        m->lib = 0;
        return;
    }

    {
        PTMutexLocker lock(o->devices_lock);
        if (o->devices.empty()) {
            // First call: init callbacks
            m->lib->registerHandler(UPNP_CONTROL_ACTION_REQUEST, 
                                    o->sCallBack, this);
            m->lib->registerHandler(UPNP_CONTROL_GET_VAR_REQUEST, 
                                    o->sCallBack, this);
            m->lib->registerHandler(UPNP_EVENT_SUBSCRIPTION_REQUEST, 
                                    o->sCallBack, this);
        }
        o->devices[m->deviceId] = this;
    }

    VirtualDir* theVD = VirtualDir::getVirtualDir();
    if (theVD == 0) {
        LOGFAT("UpnpDevice::UpnpDevice: can't get VirtualDir" << endl);
        return;
    }

    for (auto it = files.begin(); it != files.end(); it++) {
        if (!path_getsimple(it->first).compare("description.xml")) {
            m->description = it->second.content;
            break;
        }
    }
            
    if (m->description.empty()) {
        LOGFAT("UpnpDevice::UpnpDevice: no description.xml found in xmlfiles"
               << endl);
        return;
    } 

    for (auto it = files.begin(); it != files.end(); it++) {
        string dir = path_getfather(it->first);
        string fn = path_getsimple(it->first);
        // description.xml will be served by libupnp from / after inserting
        // the URLBase element (which it knows how to compute), so we make
        // sure not to serve our version from the virtual dir (if it is in /,
        // it would override libupnp's).
        if (fn.compare("description.xml")) {
            theVD->addFile(dir, fn, it->second.content, it->second.mimetype);
        }
    }
}

UpnpDevice::~UpnpDevice()
{
    UpnpUnRegisterRootDevice(m->dvh);

    PTMutexLocker lock(o->devices_lock);
    unordered_map<std::string, UpnpDevice *>::iterator it = o->devices.find(m->deviceId);
    if (it != o->devices.end())
        o->devices.erase(it);
}

bool UpnpDevice::Internal::start()
{
    // Start up the web server for sending out description files. This also
    // calls registerRootDevice()
    int ret;
    if ((ret = lib->setupWebServer(description, &dvh)) != 0) {
        LOGFAT("UpnpDevice: libupnp can't start service. Err " << ret << endl);
        return false;
    }

    if ((ret = UpnpSendAdvertisement(dvh, expiretime)) != 0) {
        LOGERR(lib->errAsString("UpnpDevice: UpnpSendAdvertisement", ret)
               << endl);
        return false;
    }
    return true;
}

// Main libupnp callback: use the device id and call the right device
int UpnpDevice::InternalStatic::sCallBack(Upnp_EventType et, void* evp, 
                                            void* tok)
{
    //LOGDEB("UpnpDevice::sCallBack" << endl);

    string deviceid;
    switch (et) {
    case UPNP_CONTROL_ACTION_REQUEST:
        deviceid = ((struct Upnp_Action_Request *)evp)->DevUDN;
    break;

    case UPNP_CONTROL_GET_VAR_REQUEST:
        deviceid = ((struct Upnp_State_Var_Request *)evp)->DevUDN;
    break;

    case UPNP_EVENT_SUBSCRIPTION_REQUEST:
        deviceid = ((struct  Upnp_Subscription_Request*)evp)->UDN;
    break;

    default:
        LOGERR("UpnpDevice::sCallBack: unknown event " << et << endl);
        return UPNP_E_INVALID_PARAM;
    }
    // LOGDEB("UpnpDevice::sCallBack: deviceid[" << deviceid << "]" << endl);

    unordered_map<std::string, UpnpDevice *>::iterator it;
    {
        PTMutexLocker lock(o->devices_lock);

        it = o->devices.find(deviceid);

        if (it == o->devices.end()) {
            LOGERR("UpnpDevice::sCallBack: Device not found: [" << 
                   deviceid << "]" << endl);
            return UPNP_E_INVALID_PARAM;
        }
    }

    // LOGDEB("UpnpDevice::sCallBack: device found: [" << it->second 
    // << "]" << endl);
    return (it->second)->m->callBack(et, evp);
}

bool UpnpDevice::ok()
{
     return o && m && m->lib != 0;
}

unordered_map<string, UpnpService*>::const_iterator 
UpnpDevice::Internal::findService(const string& serviceid)
{
    PTMutexLocker lock(devlock);
    auto servit = servicemap.find(serviceid);
    if (servit == servicemap.end()) {
        LOGERR("UpnpDevice: Bad serviceID: " << serviceid << endl);
    }
    return servit;
}

int UpnpDevice::Internal::callBack(Upnp_EventType et, void* evp)
{
    switch (et) {
    case UPNP_CONTROL_ACTION_REQUEST:
    {
        struct Upnp_Action_Request *act = (struct Upnp_Action_Request *)evp;

        LOGDEB("UPNP_CONTROL_ACTION_REQUEST: " << act->ActionName <<
               ". Params: " << ixmlwPrintDoc(act->ActionRequest) << endl);

        auto servit = findService(act->ServiceID);
        if (servit == servicemap.end()) {
            return UPNP_E_INVALID_PARAM;
        }

        SoapOutgoing dt(servit->second->getServiceType(), act->ActionName);
        {
            PTMutexLocker lock(devlock);

            auto callit = 
                calls.find(string(act->ActionName) + string(act->ServiceID));
            if (callit == calls.end()) {
                LOGINF("UpnpDevice: No such action: " << 
                       act->ActionName << endl);
                return UPNP_E_INVALID_PARAM;
            }

            SoapIncoming sc;
            if (!sc.decode(act->ActionName, act->ActionRequest)) {
                LOGERR("Error decoding Action call arguments" << endl);
                return UPNP_E_INVALID_PARAM;
            }

            // Call the action routine
            int ret = callit->second(sc, dt);
            if (ret != UPNP_E_SUCCESS) {
                LOGERR("UpnpDevice: Action failed: " << sc.getName() << endl);
                return ret;
            }
        }

        // Encode result data
        act->ActionResult = dt.buildSoapBody();

        //LOGDEB("Response data: " << ixmlwPrintDoc(act->ActionResult) << endl);

        return UPNP_E_SUCCESS;
    }
    break;

    case UPNP_CONTROL_GET_VAR_REQUEST:
        // Note that the "Control: query for variable" action is
        // deprecated (upnp arch v1), and we should never get these.
    {
        struct Upnp_State_Var_Request *act = 
            (struct Upnp_State_Var_Request *)evp;
        LOGDEB("UPNP_CONTROL_GET_VAR__REQUEST?: " << act->StateVarName << endl);
    }
    break;

    case UPNP_EVENT_SUBSCRIPTION_REQUEST:
    {
        struct Upnp_Subscription_Request *act = 
            (struct  Upnp_Subscription_Request*)evp;
        LOGDEB("UPNP_EVENT_SUBSCRIPTION_REQUEST: " << act->ServiceId << endl);

        auto servit = findService(act->ServiceId);
        if (servit == servicemap.end()) {
            return UPNP_E_INVALID_PARAM;
        }

        vector<string> names, values, qvalues;
        {
            PTMutexLocker lock(devlock);
            if (!servit->second->getEventData(true, names, values)) {
                break;
            }
        }

        vector<const char *> cnames, cvalues;
        vectorstoargslists(names, values, qvalues, cnames, cvalues);
        int ret = 
            UpnpAcceptSubscription(dvh, act->UDN, act->ServiceId,
                                   &cnames[0], &cvalues[0],
                                   int(cnames.size()), act->Sid);
        if (ret != UPNP_E_SUCCESS) {
            LOGERR(lib->errAsString(
                       "UpnpDevice::callBack: UpnpAcceptSubscription", ret) <<
                   endl);
        }

        return ret;
    }
    break;

    default:
        LOGINF("UpnpDevice::callBack: unknown libupnp event type: " <<
               LibUPnP::evTypeAsString(et).c_str() << endl);
        return UPNP_E_INVALID_PARAM;
    }
    return UPNP_E_INVALID_PARAM;
}

void UpnpDevice::addService(UpnpService *serv, const std::string& serviceId)
{
    PTMutexLocker lock(m->devlock);
    m->servicemap[serviceId] = serv;
    m->serviceids.push_back(serviceId);
}

void UpnpDevice::addActionMapping(const UpnpService* serv,
                                  const std::string& actName,
                                  soapfun fun)
{
    PTMutexLocker lock(m->devlock);
    // LOGDEB("UpnpDevice::addActionMapping:" << actName << endl);
    m->calls[actName + serv->getServiceId()] = fun;
}

void UpnpDevice::Internal::notifyEvent(const string& serviceId,
                                         const vector<string>& names, 
                                         const vector<string>& values)
{
    LOGDEB1("UpnpDevice::notifyEvent " << serviceId << " " <<
           (names.empty()?"Empty names??":names[0]) << endl);
    if (names.empty())
        return;
    vector<const char *> cnames, cvalues;
    vector<string> qvalues;
    vectorstoargslists(names, values, qvalues, cnames, cvalues);

    int ret = UpnpNotify(dvh, deviceId.c_str(), 
                         serviceId.c_str(), &cnames[0], &cvalues[0],
                         int(cnames.size()));
    if (ret != UPNP_E_SUCCESS) {
        LOGERR(lib->errAsString("UpnpDevice::notifyEvent", ret) << endl);
    }
}

int timespec_diffms(const struct timespec& old, const struct timespec& recent)
{
    return (recent.tv_sec - old.tv_sec) * 1000 + 
        (recent.tv_nsec - old.tv_nsec) / (1000 * 1000);
}

#ifndef CLOCK_REALTIME
// Mac OS X for one does not have clock_gettime. Gettimeofday is more than
// enough for our needs.
#define CLOCK_REALTIME 0
int clock_gettime(int /*clk_id*/, struct timespec* t) {
    struct timeval now;
    int rv = gettimeofday(&now, NULL);
    if (rv) return rv;
    t->tv_sec  = now.tv_sec;
    t->tv_nsec = now.tv_usec * 1000;
    return 0;
}
#endif // ! CLOCK_REALTIME

// Loop on services, and poll each for changed data. Generate event
// only if changed data exists. Every now and then, we generate an
// artificial event with all the current state. This is normally run
// by the main thread.
void UpnpDevice::eventloop()
{
    if (!m->start()) {
        LOGERR("Device would not start" << endl);
        return;
    }

    int count = 0;
    // Polling the services every 1 S
    const int loopwait_ms = 1000; 
    // Full state every 10 S. This should not be necessary, but it
    // ensures that CPs get updated about our state even if they miss
    // some events. For example, the Songcast windows sender does not
    // see the TransportState transition to "Playing" if it is not
    // repeated few seconds later, with bad consequences on further
    // operations
    const int nloopstofull = 10;  
    struct timespec wkuptime, earlytime;
    bool didearly = false;

    for (;;) {
        clock_gettime(CLOCK_REALTIME, &wkuptime);

        timespec_addnanos(&wkuptime, loopwait_ms * 1000 * 1000);

        //LOGDEB("eventloop: now " << time(0) << " wkup at "<< 
        //    wkuptime.tv_sec << " S " << wkuptime.tv_nsec << " ns" << endl);

        PTMutexLocker lock(m->evlooplock);
        int err = pthread_cond_timedwait(&m->evloopcond, lock.getMutex(), 
                                         &wkuptime);
        if (m->needExit) {
            break;
        } else if (err && err != ETIMEDOUT) {
            LOGINF("UpnpDevice:eventloop: wait errno " << errno << endl);
            break;
        } else if (err == 0) {
            // Early wakeup. Only does something if it did not already
            // happen recently
            if (didearly) {
                int millis = timespec_diffms(earlytime, wkuptime);
                if (millis < loopwait_ms) {
                    // Do nothing. didearly stays true
                    // LOGDEB("eventloop: early, previous too close "<<endl);
                    continue;
                } else {
                    // had an early wakeup previously, but it was a
                    // long time ago. Update state and wakeup
                    // LOGDEB("eventloop: early, previous is old "<<endl);
                    earlytime = wkuptime;
                }
            } else {
                // Early wakeup, previous one was normal. Remember.
                // LOGDEB("eventloop: early, no previous" << endl);
                didearly = true;
                earlytime = wkuptime;
            }
        } else {
            // Normal wakeup
            // LOGDEB("eventloop: normal wakeup" << endl);
            didearly = false;
        }

        count++;
        bool all = count && ((count % nloopstofull) == 0);
        //LOGDEB("UpnpDevice::eventloop count "<<count<<" all "<<all<<endl);

        // We can't lock devlock around the loop because we don't want
        // to hold id when calling notifyEvent() (which calls
        // libupnp). This means that we should have a separate lock
        // for the services arrays. This would only be useful during
        // startup, while we add services, but the event loop is the
        // last call the main program will make after adding the
        // services, so locking does not seem necessary
        for (auto it = m->serviceids.begin(); it != m->serviceids.end(); it++) {
            vector<string> names, values;
            {
                PTMutexLocker lock(m->devlock);
                UpnpService* serv = m->servicemap[*it];
                if (!serv->getEventData(all, names, values) || names.empty()) {
                    continue;
                }
            }
            m->notifyEvent(*it, names, values);
        }
    }
}

// Can't take the loop lock here. We're called from the service and
// hold the device lock. The locks would be taken in opposite order, 
// causing a potential deadlock:
//  - device action takes device lock
//  - loop wakes up, takes loop lock
//  - blocks on device lock before calling getevent
//  - device calls loopwakeup which blocks on loop lock
// -> deadlock
void UpnpDevice::loopWakeup()
{
    pthread_cond_broadcast(&m->evloopcond);
}

void UpnpDevice::shouldExit()
{
    m->needExit = true;
    pthread_cond_broadcast(&m->evloopcond);
}



UpnpService::UpnpService(const std::string& stp,
                         const std::string& sid, UpnpDevice *dev)
    : m_serviceType(stp), m_serviceId(sid), m(0)
{
    dev->addService(this, sid);
}

UpnpService::~UpnpService() 
{
}

bool UpnpService::getEventData(bool all, std::vector<std::string>& names, 
                               std::vector<std::string>& values) 
{
    return true;
}

const std::string& UpnpService::getServiceType() const 
{
    return m_serviceType;
}

const std::string& UpnpService::getServiceId() const 
{
    return m_serviceId;
}


}// End namespace UPnPProvider
