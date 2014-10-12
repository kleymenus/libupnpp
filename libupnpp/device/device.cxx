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

#include "vdir.hxx"                     // for VirtualDir

using namespace std;
using namespace UPnPP;

namespace UPnPProvider {

unordered_map<std::string, UpnpDevice *> UpnpDevice::o_devices;
PTMutexInit o_devices_lock;

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
    : m_deviceId(deviceId), m_needExit(false), m_evloopcond(PTHREAD_COND_INITIALIZER)
{
    //LOGDEB("UpnpDevice::UpnpDevice(" << m_deviceId << ")" << endl);

    m_lib = LibUPnP::getLibUPnP(true);
    if (!m_lib) {
        LOGFAT(" Can't get LibUPnP" << endl);
        return;
    }
    if (!m_lib->ok()) {
        LOGFAT("Lib init failed: " <<
               m_lib->errAsString("main", m_lib->getInitError()) << endl);
        m_lib = 0;
        return;
    }

    {
        PTMutexLocker lock(o_devices_lock);
        if (o_devices.empty()) {
            // First call: init callbacks
            m_lib->registerHandler(UPNP_CONTROL_ACTION_REQUEST, sCallBack, this);
            m_lib->registerHandler(UPNP_CONTROL_GET_VAR_REQUEST, sCallBack, this);
            m_lib->registerHandler(UPNP_EVENT_SUBSCRIPTION_REQUEST, sCallBack,this);
        }
        o_devices[m_deviceId] = this;
    }

    VirtualDir* theVD = VirtualDir::getVirtualDir();
    if (theVD == 0) {
        LOGFAT("UpnpDevice::UpnpDevice: can't get VirtualDir" << endl);
        return;
    }

    unordered_map<string, VDirContent>::const_iterator it = 
        files.find("description.xml");
    if (it == files.end()) {
        LOGFAT("UpnpDevice::UpnpDevice: no description.xml found in xmlfiles"
               << endl);
        return;
    } 

    const string& description = it->second.content;

    for (it = files.begin(); it != files.end(); it++) {
        theVD->addFile("/", it->first, it->second.content, it->second.mimetype);
    }

    // Start up the web server for sending out description files
    int ret;
    if ((ret = m_lib->setupWebServer(description, &m_dvh)) != 0) {
        LOGFAT("UpnpDevice: libupnp can't start service. Err " << ret << endl);
    }

    if ((ret = UpnpSendAdvertisement(m_dvh, expiretime)) != 0) {
        LOGERR(m_lib->errAsString("UpnpDevice: UpnpSendAdvertisement", ret)
               << endl);
    }
}

UpnpDevice::~UpnpDevice()
{
    UpnpUnRegisterRootDevice(m_dvh);

    PTMutexLocker lock(o_devices_lock);
    unordered_map<std::string, UpnpDevice *>::iterator it = o_devices.find(m_deviceId);
    if (it != o_devices.end())
        o_devices.erase(it);
}

// Main libupnp callback: use the device id and call the right device
int UpnpDevice::sCallBack(Upnp_EventType et, void* evp, void* tok)
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
        PTMutexLocker lock(o_devices_lock);

        it = o_devices.find(deviceid);

        if (it == o_devices.end()) {
            LOGERR("UpnpDevice::sCallBack: Device not found: [" << 
                   deviceid << "]" << endl);
            return UPNP_E_INVALID_PARAM;
        }
    }

    // LOGDEB("UpnpDevice::sCallBack: device found: [" << it->second 
    // << "]" << endl);
    return (it->second)->callBack(et, evp);
}

unordered_map<string, UpnpService*>::const_iterator UpnpDevice::findService(const string& serviceid)
{
    PTMutexLocker lock(m_lock);
    auto servit = m_servicemap.find(serviceid);
    if (servit == m_servicemap.end()) {
        LOGERR("UpnpDevice: Bad serviceID: " << serviceid << endl);
    }
    return servit;
}

int UpnpDevice::callBack(Upnp_EventType et, void* evp)
{
    switch (et) {
    case UPNP_CONTROL_ACTION_REQUEST:
    {
        struct Upnp_Action_Request *act = (struct Upnp_Action_Request *)evp;

        LOGDEB("UPNP_CONTROL_ACTION_REQUEST: " << act->ActionName <<
               ". Params: " << ixmlwPrintDoc(act->ActionRequest) << endl);

        auto servit = findService(act->ServiceID);
        if (servit == m_servicemap.end()) {
            return UPNP_E_INVALID_PARAM;
        }

        SoapData dt;
        {
            PTMutexLocker lock(m_lock);
            const string& servicetype = servit->second->getServiceType();
            auto callit = m_calls.find(string(act->ActionName) + string(act->ServiceID));
            if (callit == m_calls.end()) {
                LOGINF("UpnpDevice: No such action: " << act->ActionName << endl);
                return UPNP_E_INVALID_PARAM;
            }

            SoapArgs sc;
            if (!decodeSoapBody(act->ActionName, act->ActionRequest, &sc)) {
                LOGERR("Error decoding Action call arguments" << endl);
                return UPNP_E_INVALID_PARAM;
            }
            dt.name = act->ActionName;
            dt.serviceType = servicetype;

            // Call the action routine
            int ret = callit->second(sc, dt);
            if (ret != UPNP_E_SUCCESS) {
                LOGERR("UpnpDevice: Action failed: " << sc.name << endl);
                return ret;
            }
        }

        // Encode result data
        act->ActionResult = buildSoapBody(dt);

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
        if (servit == m_servicemap.end()) {
            return UPNP_E_INVALID_PARAM;
        }

        vector<string> names, values, qvalues;
        {
            PTMutexLocker lock(m_lock);
            if (!servit->second->getEventData(true, names, values)) {
                break;
            }
        }

        vector<const char *> cnames, cvalues;
        vectorstoargslists(names, values, qvalues, cnames, cvalues);
        int ret = 
            UpnpAcceptSubscription(m_dvh, act->UDN, act->ServiceId,
                                   &cnames[0], &cvalues[0],
                                   int(cnames.size()), act->Sid);
        if (ret != UPNP_E_SUCCESS) {
            LOGERR(m_lib->errAsString(
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
    PTMutexLocker lock(m_lock);
    m_servicemap[serviceId] = serv;
    m_serviceids.push_back(serviceId);
}

void UpnpDevice::addActionMapping(const UpnpService* serv,
                                  const std::string& actName,
                                  soapfun fun)
{
    PTMutexLocker lock(m_lock);
    // LOGDEB("UpnpDevice::addActionMapping:" << actName << endl);
    m_calls[actName + serv->getServiceId()] = fun;
}

void UpnpDevice::notifyEvent(const string& serviceId,
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

    int ret = UpnpNotify(m_dvh, m_deviceId.c_str(), 
                         serviceId.c_str(), &cnames[0], &cvalues[0],
                         int(cnames.size()));
    if (ret != UPNP_E_SUCCESS) {
        LOGERR(m_lib->errAsString("UpnpDevice::notifyEvent", ret) << endl);
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
// artificial event with all the current state. This is normally run by the main thread.
void UpnpDevice::eventloop()
{
    int count = 0;
    const int loopwait_ms = 1000; // Polling the services every 1 S
    const int nloopstofull = 10;  // Full state every 10 S
    struct timespec wkuptime, earlytime;
    bool didearly = false;

    for (;;) {
        clock_gettime(CLOCK_REALTIME, &wkuptime);

        timespec_addnanos(&wkuptime, loopwait_ms * 1000 * 1000);

        //LOGDEB("eventloop: now " << time(0) << " wkup at "<< 
        //    wkuptime.tv_sec << " S " << wkuptime.tv_nsec << " ns" << endl);

        PTMutexLocker lock(m_evlooplock);
        int err = pthread_cond_timedwait(&m_evloopcond, lock.getMutex(), 
                                         &wkuptime);
        if (m_needExit) {
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

        // We can't lock m_lock around the loop because we don't want
        // to hold id when calling notifyEvent() (which calls
        // libupnp). This means that we should have a separate lock
        // for the services arrays. This would only be useful during
        // startup, while we add services, but the event loop is the
        // last call the main program will make after adding the
        // services, so locking does not seem necessary
        for (auto it = m_serviceids.begin(); it != m_serviceids.end(); it++) {
            vector<string> names, values;
            {
                PTMutexLocker lock(m_lock);
                UpnpService* serv = m_servicemap[*it];
                if (!serv->getEventData(all, names, values) || names.empty()) {
                    continue;
                }
            }
            notifyEvent(*it, names, values);
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
    pthread_cond_broadcast(&m_evloopcond);
}

void UpnpDevice::shouldExit()
{
    m_needExit = true;
    pthread_cond_broadcast(&m_evloopcond);
}

}// End namespace UPnPProvider
