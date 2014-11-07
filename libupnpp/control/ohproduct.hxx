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
#ifndef _OHPRODUCT_HXX_INCLUDED_
#define _OHPRODUCT_HXX_INCLUDED_

#include <memory>                       // for shared_ptr
#include <string>                       // for string
#include <vector>                       // for vector

#include "service.hxx"                  // for Service

namespace UPnPClient { class OHProduct; }
namespace UPnPClient { class UPnPDeviceDesc; }
namespace UPnPClient { class UPnPServiceDesc; }

namespace UPnPClient {

typedef std::shared_ptr<OHProduct> OHPRH;

/**
 * OHProduct Service client class.
 *
 */
class OHProduct : public Service {
public:

    OHProduct(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
        : Service(device, service) {
    }

    OHProduct() {}

    /** Test service type from discovery message */
    static bool isOHPrService(const std::string& st);

    struct Source {
        std::string name;
        std::string type;
        bool visible;
        Source() : visible(false) {}
        void clear() {name.clear(); type.clear(); visible = false;}
    };

    /** @ret 0 for success, upnp error else */
    int getSources(std::vector<Source>& sources);

protected:
    /* My service type string */
    static const std::string SType;
};

} // namespace UPnPClient

#endif /* _OHPRODUCT_HXX_INCLUDED_ */
