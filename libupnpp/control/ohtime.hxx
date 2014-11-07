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
#ifndef _OHTIME_HXX_INCLUDED_
#define _OHTIME_HXX_INCLUDED_

#include <memory>                       // for shared_ptr
#include <string>                       // for string
#include <unordered_map>                // for unordered_map
#include <vector>                       // for vector

#include "cdircontent.hxx"              // for UPnPDirObject
#include "service.hxx"                  // for Service

namespace UPnPClient { class OHTime; }
namespace UPnPClient { class UPnPDeviceDesc; }
namespace UPnPClient { class UPnPServiceDesc; }

namespace UPnPClient {

typedef std::shared_ptr<OHTime> OHTMH;

/**
 * OHTime Service client class.
 *
 */
class OHTime : public Service {
public:

    OHTime(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
        : Service(device, service) {
        registerCallback();
    }
    virtual ~OHTime() {
        unregisterCallback();
    }

    OHTime() {}

    /** Test service type from discovery message */
    static bool isOHTMService(const std::string& st);

    struct Time {
        int trackCount;
        int duration;
        int seconds;
    };
    int time(Time&);

protected:
    /* My service type string */
    static const std::string SType;

private:
    void evtCallback(const std::unordered_map<std::string, std::string>&);
    void registerCallback();
};

} // namespace UPnPClient

#endif /* _OHTIME_HXX_INCLUDED_ */
