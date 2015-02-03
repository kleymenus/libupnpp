/* Copyright (C) 2013 J.F.Dockes
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
#include "config.h"

#include "discovery.hxx"

#include <pthread.h>                    // for pthread_cond_broadcast, etc
#include <sched.h>                      // for sched_yield
#include <stdlib.h>                     // for free
#include <time.h>                       // for CLOCK_REALTIME
#include <sys/time.h>
#include <stdio.h>
 
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include <unistd.h>                     // for sleep
#include <upnp/upnp.h>                  // for Upnp_Discovery, etc

#include <functional>                   // for _Bind, bind, function, _1, etc
#include <iostream>                     // for operator<<, basic_ostream, etc
#include <map>                          // for _Rb_tree_iterator, map, etc
#include <utility>                      // for pair
#include <vector>                       // for vector
#include <unordered_set>

#include "description.hxx"              // for UPnPDeviceDesc, etc

#include "libupnpp/log.hxx"             // for LOGDEB1, LOGERR, LOGDEB
#include "libupnpp/ptmutex.hxx"         // for PTMutexLocker, PTMutexInit
#include "libupnpp/upnpplib.hxx"        // for LibUPnP
#include "libupnpp/upnpputils.hxx"      // for timespec_addnanos
#include "libupnpp/workqueue.hxx"       // for WorkQueue
#include "libupnpp/control/httpdownload.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

static UPnPDeviceDirectory *theDevDir;

//#undef LOCAL_LOGINC
//#define LOCAL_LOGINC 3

static string cluDiscoveryToStr(const struct Upnp_Discovery *disco)
{
    stringstream ss;
    ss << "ErrCode: " << disco->ErrCode << endl;
    ss << "Expires: " << disco->Expires << endl;
    ss << "DeviceId: " << disco->DeviceId << endl;
    ss << "DeviceType: " << disco->DeviceType << endl;
    ss << "ServiceType: " << disco->ServiceType << endl;
    ss << "ServiceVer: " << disco->ServiceVer    << endl;
    ss << "Location: " << disco->Location << endl;
    ss << "Os: " << disco->Os << endl;
    ss << "Date: " << disco->Date << endl;
    ss << "Ext: " << disco->Ext << endl;

    /** The host address of the device responding to the search. */
    // struct sockaddr_storage DestAddr;
    return ss.str();
}

// Each appropriate discovery event (executing in a libupnp thread
// context) queues the following task object for processing by the
// discovery thread.
class DiscoveredTask {
public:
    DiscoveredTask(bool _alive, const struct Upnp_Discovery *disco)
        : alive(_alive), url(disco->Location), deviceId(disco->DeviceId),
          expires(disco->Expires)
        {}

    bool alive;
    string url;
    string description;
    string deviceId;
    int expires; // Seconds valid
};

// The workqueue on which callbacks from libupnp (cluCallBack()) queue
// discovered object descriptors for processing by our dedicated
// thread.
static WorkQueue<DiscoveredTask*> discoveredQueue("DiscoveredQueue");
static unordered_set<string> o_downloading;
static PTMutexInit o_downloading_mutex;

// This gets called in a libupnp thread context for all asynchronous
// events which we asked for.
// Example: ContentDirectories appearing and disappearing from the network
// We queue a task for our worker thread(s)
// We can get called by several threads.
static int cluCallBack(Upnp_EventType et, void* evp, void*)
{
    LOGDEB1("discovery:cluCallBack: " << LibUPnP::evTypeAsString(et) << endl);

    switch (et) {
    case UPNP_DISCOVERY_SEARCH_RESULT:
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
    {
        struct Upnp_Discovery *disco = (struct Upnp_Discovery *)evp;
        // Devices send multiple messages for themselves, their subdevices and 
        // services. AFAIK they all point to the same description.xml document,
        // which has all the interesting data. So let's try to only process
        // one message per device: the one which probably correspond to the 
        // upnp "root device" message and has empty service and device types:
        if (!disco->DeviceType[0] && !disco->ServiceType[0]) {
            LOGDEB1("discovery:cllb:ALIVE: " << cluDiscoveryToStr(disco) 
                   << endl);
            // Device signals its existence and well-being. Perform the
            // UPnP "description" phase by downloading and decoding the
            // description document.

            DiscoveredTask *tp = new DiscoveredTask(1, disco);

            {
                // Note that this does not prevent multiple successive
                // downloads of a normal url, just multiple
                // simultaneous downloads of a slow one, to avoid
                // tying up threads.
                PTMutexLocker lock(o_downloading_mutex);
                auto res = o_downloading.insert(tp->url);
                if (!res.second) {
                    LOGDEB("discovery:cllb: already downloading " << 
                           tp->url << endl);
                    return UPNP_E_SUCCESS;
                }
            }

            LOGDEB("discoExplorer: downloading " << tp->url << endl);
            string sdesc;
            if (!downloadUrlWithCurl(tp->url, tp->description, 5)) {
                LOGERR("discovery:cllb: downloadUrlWithCurl error for: " << 
                       tp->url << endl);
                {PTMutexLocker lock(o_downloading_mutex);
                    o_downloading.erase(tp->url);
                }
                delete tp;
                return UPNP_E_SUCCESS;
            }
            LOGDEB1("discovery:cllb: downloaded description document of " <<
                    tp->description.size() << " bytes" << endl);

            {PTMutexLocker lock(o_downloading_mutex);
                o_downloading.erase(tp->url);
            }

            if (discoveredQueue.put(tp)) {
                return UPNP_E_FINISH;
            }
        }
        break;
    }
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
    {
        struct Upnp_Discovery *disco = (struct Upnp_Discovery *)evp;
        //LOGDEB("discovery:cllB:BYEBYE: " << cluDiscoveryToStr(disco) << endl);
        DiscoveredTask *tp = new DiscoveredTask(0, disco);
        if (discoveredQueue.put(tp)) {
            return UPNP_E_FINISH;
        }
        break;
    }
    default:
        // Ignore other events for now
        LOGDEB("discovery:cluCallBack: unprocessed evt type: [" << 
               LibUPnP::evTypeAsString(et) << "]"  << endl);
        break;
    }

    return UPNP_E_SUCCESS;
}

// Our client can set up functions to be called when we process a new device.
// This is used during startup, when the pool is not yet complete, to enable
// finding and listing devices as soon as they appear.
static vector<UPnPDeviceDirectory::Visitor> o_callbacks;
static PTMutexInit o_callbacks_mutex;

unsigned int UPnPDeviceDirectory::addCallback(UPnPDeviceDirectory::Visitor v)
{
    PTMutexLocker lock(o_callbacks_mutex);
    o_callbacks.push_back(v);
    return o_callbacks.size() - 1;
}

void UPnPDeviceDirectory::delCallback(unsigned int idx)
{
    PTMutexLocker lock(o_callbacks_mutex);
    if (idx >= o_callbacks.size())
        return;
    o_callbacks.erase(o_callbacks.begin() + idx);
}

// Descriptor kept in the device pool for each device found on the network.
class DeviceDescriptor {
public:
    DeviceDescriptor(const string& url, const string& description,
                     time_t last, int exp)
        : device(url, description), last_seen(last), expires(exp+20)
        {}
    DeviceDescriptor()
        {}
    UPnPDeviceDesc device;
    time_t last_seen;
    int expires; // seconds valid
};

// A DevicePool holds the characteristics of the devices
// currently on the network.
// The map is referenced by deviceId (==UDN)
// The class is instanciated as a static (unenforced) singleton.
class DevicePool {
public:
    PTMutexInit m_mutex;
    map<string, DeviceDescriptor> m_devices;
};
static DevicePool o_pool;
typedef map<string, DeviceDescriptor>::iterator DevPoolIt;

// Worker routine for the discovery queue. Get messages about devices
// appearing and disappearing, and update the directory pool
// accordingly.
void *UPnPDeviceDirectory::discoExplorer(void *)
{
    for (;;) {
        DiscoveredTask *tsk = 0;
        size_t qsz;
        if (!discoveredQueue.take(&tsk, &qsz)) {
            discoveredQueue.workerExit();
            return (void*)1;
        }
        LOGDEB1("discoExplorer: got task: alive " << tsk->alive << " deviceId ["
                << tsk->deviceId << " URL [" << tsk->url << "]" << endl);

        if (!tsk->alive) {
            // Device signals it is going off.
            PTMutexLocker lock(o_pool.m_mutex);
            DevPoolIt it = o_pool.m_devices.find(tsk->deviceId);
            if (it != o_pool.m_devices.end()) {
                o_pool.m_devices.erase(it);
                //LOGDEB("discoExplorer: delete " << tsk->deviceId.c_str() << 
                // endl);
            }
        } else {
            // Update or insert the device
            DeviceDescriptor d(tsk->url, tsk->description, time(0), tsk->expires);
            if (!d.device.ok) {
                LOGERR("discoExplorer: description parse failed for " << 
                       tsk->deviceId << endl);
                delete tsk;
                continue;
            }
            LOGDEB1("discoExplorer: found id [" << tsk->deviceId  << "]" 
                    << " name " << d.device.friendlyName 
                    << " devtype " << d.device.deviceType << endl);
            {
                PTMutexLocker lock(o_pool.m_mutex);
                //LOGDEB1("discoExplorer: inserting device id "<< tsk->deviceId
                // <<  " description: " << endl << d.device.dump() << endl);
                o_pool.m_devices[tsk->deviceId] = d;
            }
            {
                PTMutexLocker lock(o_callbacks_mutex);
                for (auto cbp = o_callbacks.begin(); 
                     cbp != o_callbacks.end(); cbp++) {
                    (*cbp)(d.device, UPnPServiceDesc());
                }
            }
        }
        delete tsk;
    }
}

// Look at the devices and get rid of those which have not been seen
// for too long. We do this when listing the top directory
void UPnPDeviceDirectory::expireDevices()
{
    LOGDEB1("discovery: expireDevices:" << endl);
    PTMutexLocker lock(o_pool.m_mutex);
    time_t now = time(0);
    bool didsomething = false;

    for (DevPoolIt it = o_pool.m_devices.begin();
         it != o_pool.m_devices.end();) {
        LOGDEB1("Dev in pool: type: " << it->second.device.deviceType <<
                " friendlyName " << it->second.device.friendlyName << endl);
        if (now - it->second.last_seen > it->second.expires) {
            //LOGDEB("expireDevices: deleting " <<  it->first.c_str() << " " << 
            //   it->second.device.friendlyName.c_str() << endl);
            o_pool.m_devices.erase(it++);
            didsomething = true;
        } else {
            it++;
        }
    }
    if (didsomething)
        search();
}

// m_searchTimeout is the UPnP device search timeout, which should
// actually be called delay because it's the base of a random delay
// that the devices apply to avoid responding all at the same time.
// This means that you have to wait for the specified period before
// the results are complete.
UPnPDeviceDirectory::UPnPDeviceDirectory(time_t search_window)
    : m_ok(false), m_searchTimeout(search_window), m_lastSearch(0)
{
    addCallback(std::bind(&UPnPDeviceDirectory::deviceFound, this, _1, _2));

    if (!discoveredQueue.start(1, discoExplorer, 0)) {
        m_reason = "Discover work queue start failed";
        return;
    }
    sched_yield();
    LibUPnP *lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        m_reason = "Can't get lib";
        return;
    }
    lib->registerHandler(UPNP_DISCOVERY_SEARCH_RESULT, cluCallBack, this);
    lib->registerHandler(UPNP_DISCOVERY_ADVERTISEMENT_ALIVE,
                         cluCallBack, this);
    lib->registerHandler(UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE,
                         cluCallBack, this);

    m_ok = search();
}

bool UPnPDeviceDirectory::search()
{
    LOGDEB1("UPnPDeviceDirectory::search" << endl);
    if (time(0) - m_lastSearch < 10)
        return true;

    LibUPnP *lib = LibUPnP::getLibUPnP();
    if (lib == 0) {
        m_reason = "Can't get lib";
        return false;
    }

    LOGDEB1("UPnPDeviceDirectory::search: calling upnpsearchasync"<<endl);
    //const char *cp = "ssdp:all";
    const char *cp = "upnp:rootdevice";
    int code1 = UpnpSearchAsync(lib->getclh(), m_searchTimeout, cp, lib);
    if (code1 != UPNP_E_SUCCESS) {
        m_reason = LibUPnP::errAsString("UpnpSearchAsync", code1);
        LOGERR("UPnPDeviceDirectory::search: UpnpSearchAsync failed: " <<
               m_reason << endl);
    }
    m_lastSearch = time(0);
    return true;
}

UPnPDeviceDirectory *UPnPDeviceDirectory::getTheDir(time_t search_window)
{
    if (theDevDir == 0)
        theDevDir = new UPnPDeviceDirectory(search_window);
    if (theDevDir && !theDevDir->ok())
        return 0;
    return theDevDir;
}

void UPnPDeviceDirectory::terminate()
{
    discoveredQueue.setTerminateAndWait();
}

time_t UPnPDeviceDirectory::getRemainingDelay()
{
    time_t now = time(0);
    if (now - m_lastSearch >= m_searchTimeout)
        return 0;
    return  m_searchTimeout - (now - m_lastSearch);
}

bool UPnPDeviceDirectory::traverse(UPnPDeviceDirectory::Visitor visit)
{
    //LOGDEB("UPnPDeviceDirectory::traverse" << endl);
    if (m_ok == false)
        return false;
    int secs = getRemainingDelay();
    if (secs > 0)
        sleep(secs);

    // Has locking, do it before our own lock
    expireDevices();

    PTMutexLocker lock(o_pool.m_mutex);

    for (auto it = o_pool.m_devices.begin(); 
         it != o_pool.m_devices.end(); it++) {
        for (auto it1 = it->second.device.services.begin();
             it1 != it->second.device.services.end(); it1++) {
            if (!visit(it->second.device, *it1))
                return false;
        }
    }
    return true;
}

static PTMutexInit devWaitLock;
static pthread_cond_t devWaitCond = PTHREAD_COND_INITIALIZER;

bool UPnPDeviceDirectory::deviceFound(const UPnPDeviceDesc&, 
                                      const UPnPServiceDesc&)
{
    PTMutexLocker lock(devWaitLock);
    pthread_cond_broadcast(&devWaitCond);
    return true;
}

bool UPnPDeviceDirectory::getDevBySelector(bool cmp(const UPnPDeviceDesc& ddesc,
                                                    const string&), 
                                           const string& value,
                                           UPnPDeviceDesc& ddesc)
{
    // Has locking, do it before our own lock
    expireDevices();

    struct timespec wkuptime;
    long long nanos = getRemainingDelay() * 1000*1000*1000;
    
    #ifdef __MACH__ // Mac OS X does not have clock_gettime, use clock_get_time
      clock_serv_t cclock;
      mach_timespec_t mts;
      host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
      clock_get_time(cclock, &mts);
      mach_port_deallocate(mach_task_self(), cclock);
      wkuptime.tv_sec = mts.tv_sec;
      wkuptime.tv_nsec = mts.tv_nsec;
    #else
      clock_gettime(CLOCK_REALTIME, &wkuptime);
    #endif

    UPnPP::timespec_addnanos(&wkuptime, nanos);
    do {
        PTMutexLocker lock(devWaitLock);
        {
            PTMutexLocker lock(o_pool.m_mutex);
            for (auto it = o_pool.m_devices.begin(); 
                 it != o_pool.m_devices.end(); it++) {
                if (!cmp(it->second.device, value)) {
                    ddesc = it->second.device;
                    return true;
                }
            }
        }

        if (nanos > 0) {
            pthread_cond_timedwait(&devWaitCond, lock.getMutex(), &wkuptime);
        }
    } while (getRemainingDelay() > 0);
    return false;
}

static bool cmpFName(const UPnPDeviceDesc& ddesc, const string& fname)
{
    return ddesc.friendlyName.compare(fname);
}

bool UPnPDeviceDirectory::getDevByFName(const string& fname, 
                                        UPnPDeviceDesc& ddesc)
{
    return getDevBySelector(cmpFName, fname, ddesc);
}

static bool cmpUDN(const UPnPDeviceDesc& ddesc, const string& value)
{
    return ddesc.UDN.compare(value);
}

bool UPnPDeviceDirectory::getDevByUDN(const string& value, 
                                      UPnPDeviceDesc& ddesc)
{
    return getDevBySelector(cmpUDN, value, ddesc);
}



} // namespace UPnPClient
