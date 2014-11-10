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
#ifndef _SOAPHELP_H_X_INCLUDED_
#define _SOAPHELP_H_X_INCLUDED_

#include <upnp/ixml.h>                  // for IXML_Document

#include <map>                          // for map
#include <string>                       // for string
#include <unordered_map>                // for unordered_map
#include <utility>                      // for pair
#include <vector>                       // for vector

namespace UPnPP {

/** Decode incoming Soap call data */
class SoapIncoming {
public:
    SoapIncoming() : m_ok(false) {
    }

    /** Construct by decoding the XML passed from libupnp. Call ok() to check
     * if this went well.
     *
     * @param name We could get this from the XML doc, but get caller
     *    gets it from libupnp, and passing it is simpler than retrieving
     *    from the input top node where it has a namespace qualifier.
     * @param actReq the XML document containing the SOAP data.
     */
    bool decode(const char *name, IXML_Document *actReq);

    // Get call name 
    const std::string& getName() const {
        return m_name;
    }
    bool getBool(const char *nm, bool *value) const;
    bool getInt(const char *nm, int *value) const;
    bool getString(const char *nm, std::string *value) const;
    bool get(const char *nm, bool *value) const {
        return getBool(nm, value);
    }
    bool get(const char *nm, int *value) const {
        return getInt(nm, value);
    }
    bool get(const char *nm, std::string *value) const {
        return getString(nm, value);
    }
    bool ok() {
        return m_ok;
    }
private:
    std::string m_name;
    std::map<std::string, std::string> m_args;
    bool m_ok;
};

namespace SoapHelp {
    std::string xmlQuote(const std::string& in);
    std::string xmlUnquote(const std::string& in);
    std::string i2s(int val);
    inline std::string val2s(const std::string& val) {return val;}
    inline std::string val2s(int val) {return i2s(val);}
    inline std::string val2s(bool val) {return i2s(int(val));}
};

/** Store the values to be encoded in a SOAP response. 
 *
 * The elements in the response must be in a defined order, so we
 * can't use a map as container, we use a vector of pairs instead.
 * The generic UpnpDevice callback fills up name and service type, the
 * device call only needs to fill the data vector.
 */
class SoapOutgoing {
public:
    SoapOutgoing() {}
    SoapOutgoing(const std::string& st, const std::string& nm)
        : m_serviceType(st), m_name(nm) {}

    SoapOutgoing& addarg(const std::string& k, const std::string& v) {
        m_data.push_back(std::pair<std::string, std::string>(k, v));
        return *this;
    }

    SoapOutgoing& operator() (const std::string& k, const std::string& v) {
        m_data.push_back(std::pair<std::string, std::string>(k, v));
        return *this;
    }

    /** Build the SOAP call or response data XML document from the
       vector of named values */
    IXML_Document *buildSoapBody(bool isResp = true) const;

    const std::string& getName() const {
        return m_name;
    }

    std::string i2s(int val) {
        return SoapHelp::i2s(val);
    }

private:
    std::string m_serviceType;
    std::string m_name;
    std::vector<std::pair<std::string, std::string> > m_data;
};


/** Decode UPnP Event data. This is not soap, but it's quite close to
 *  the other code in here so whatever...
 *
 * The variable values are contained in a propertyset XML document:
 *     <?xml version="1.0"?>
 *     <e:propertyset xmlns:e="urn:schemas-upnp-org:event-1-0">
 *       <e:property>
 *         <variableName>new value</variableName>
 *       </e:property>
 *       <!-- Other variable names and values (if any) go here. -->
 *     </e:propertyset>
 */
extern bool decodePropertySet(IXML_Document *doc, 
                       std::unordered_map<std::string,std::string>& out);


} // namespace UPnPP

#endif /* _SOAPHELP_H_X_INCLUDED_ */
