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
// An XML parser which constructs an UPnP device object from the
// device descriptor
#include "config.h"

#include "description.hxx"

#include <unordered_map>

#include <expat_external.h>             // for XML_Char
#include <string.h>                     // for strcmp
#include <upnp/upnp.h>                  // for UpnpDownload...

#include "libupnpp/upnpplib.hxx"        
#include "libupnpp/expatmm.hxx"         // for inputRefXMLParser
#include "libupnpp/upnpp_p.hxx"         // for baseurl, trimstring
#include "libupnpp/log.hxx"

using namespace std;
using namespace UPnPP;

namespace UPnPClient {

class UPnPDeviceParser : public inputRefXMLParser {
public:
    UPnPDeviceParser(const string& input, UPnPDeviceDesc& device)
        : inputRefXMLParser(input), m_device(device)
        {}

protected:
    virtual void StartElement(const XML_Char *name, const XML_Char **)
	{
            m_tabs.push_back('\t');
            m_path.push_back(name);
	}
    virtual void EndElement(const XML_Char *name)
	{
            if (!strcmp(name, "service")) {
                m_device.services.push_back(m_tservice);
                m_tservice.clear();
            }
            if (m_tabs.size())
                m_tabs.erase(m_tabs.size()-1);
            m_path.pop_back();
	}
    virtual void CharacterData(const XML_Char *s, int len)
	{
            if (s == 0 || *s == 0)
                return;
            string str(s, len);
            trimstring(str);
            switch (m_path.back()[0]) {
            case 'c':
                if (!m_path.back().compare("controlURL"))
                    m_tservice.controlURL += str;
                break;
            case 'd':
                if (!m_path.back().compare("deviceType"))
                    m_device.deviceType += str;
                break;
            case 'e':
                if (!m_path.back().compare("eventSubURL"))
                    m_tservice.eventSubURL += str;
                break;
            case 'f':
                if (!m_path.back().compare("friendlyName"))
                    m_device.friendlyName += str;
                break;
            case 'm':
                if (!m_path.back().compare("manufacturer"))
                    m_device.manufacturer += str;
                else if (!m_path.back().compare("modelName"))
                    m_device.modelName += str;
                break;
            case 's':
                if (!m_path.back().compare("serviceType"))
                    m_tservice.serviceType = str;
                else if (!m_path.back().compare("serviceId"))
                    m_tservice.serviceId += str;
            case 'S':
                if (!m_path.back().compare("SCPDURL"))
                    m_tservice.SCPDURL = str;
                break;
            case 'U':
                if (!m_path.back().compare("UDN"))
                    m_device.UDN = str;
                else if (!m_path.back().compare("URLBase"))
                    m_device.URLBase += str;
                break;
            }
	}

private:
    UPnPDeviceDesc& m_device;
    string m_tabs;
    std::vector<std::string> m_path;
    UPnPServiceDesc m_tservice;
};

UPnPDeviceDesc::UPnPDeviceDesc(const string& url, const string& description)
    : ok(false)
{
    //cerr << "UPnPDeviceDesc::UPnPDeviceDesc: url: " << url << endl;
    //cerr << " description " << endl << description << endl;

    UPnPDeviceParser mparser(description, *this);
    if (!mparser.Parse())
        return;
    if (URLBase.empty()) {
        // The standard says that if the URLBase value is empty, we
        // should use the url the description was retrieved
        // from. However this is sometimes something like
        // http://host/desc.xml, sometimes something like http://host/
        // (rare, but e.g. sent by the server on a dlink nas).
        URLBase = baseurl(url);
    }
    ok = true;
    //cerr << "URLBase: [" << URLBase << "]" << endl;
    //cerr << dump() << endl;
}




// XML parser for the service description document (SCPDURL)
class ServiceDescriptionParser : public inputRefXMLParser {
public:
    ServiceDescriptionParser(UPnPServiceDesc::Parsed& out, const string& input)
        : inputRefXMLParser(input), m_parsed(out)
    {
    }

protected:
    class StackEl {
    public:
        StackEl(const string& nm) : name(nm) {}
        string name;
        XML_Size sta;
        unordered_map<string,string> attributes;
        string data;
    };

    virtual void StartElement(const XML_Char *name, const XML_Char **attrs)
    {
        //LOGDEB("startElement: name [" << name << "]" << " bpos " <<
        //             XML_GetCurrentByteIndex(expat_parser) << endl);

        m_path.push_back(StackEl(name));
        StackEl& lastelt = m_path.back();
        lastelt.sta = XML_GetCurrentByteIndex(expat_parser);
        for (int i = 0; attrs[i] != 0; i += 2) {
            lastelt.attributes[attrs[i]] = attrs[i+1];
        }

        switch (name[0]) {
        case 'a':
            if (!strcmp(name, "action")) {
                m_tact.clear();
            } else if (!strcmp(name, "argument")) {
                m_targ.clear();
            }
            break;
        case 's':
            if (!strcmp(name, "stateVariable")) {
                m_tvar.clear();
                auto it = lastelt.attributes.find("sendEvents");
                if (it != lastelt.attributes.end()) {
                    stringToBool(it->second, &m_tvar.sendEvents);
                }
            }
            break;
        default:
            break;
        }
    }

    virtual void EndElement(const XML_Char *name)
    {
        string parentname;
        if (m_path.size() == 1) {
            parentname = "root";
        } else {
            parentname = m_path[m_path.size()-2].name;
        }
        StackEl& lastelt = m_path.back();
        //LOGINF("ServiceDescriptionParser: Closing element " << name 
        //<< " inside element " << parentname << 
        //" data " << m_path.back().data << endl);

        switch (name[0]) { 
        case 'a':
        if (!strcmp(name, "action")) {
            m_parsed.actionList[m_tact.name] = m_tact;
        } else if (!strcmp(name, "argument")) {
            m_tact.argList.push_back(m_targ);
        } 
        break;
        case 'd':
            if (!strcmp(name, "direction")) {
                if (!lastelt.data.compare("in")) {
                    m_targ.todevice = false;
                } else {
                    m_targ.todevice = true;
                }
            } else if (!strcmp(name, "dataType")) {
                m_tvar.dataType = lastelt.data;
                trimstring(m_tvar.dataType);
            }
            break;
        case 'm':
            if (!strcmp(name, "minimum")) {
                m_tvar.hasValueRange = true;
                m_tvar.minimum = atoi(lastelt.data.c_str());
            } else if (!strcmp(name, "maximum")) {
                m_tvar.hasValueRange = true;
                m_tvar.maximum = atoi(lastelt.data.c_str());
            }
            break;
        case 'n':
            if (!strcmp(name, "name")) {
                if (!parentname.compare("argument")) {
                    m_targ.name = lastelt.data;
                    trimstring(m_targ.name);
                } else if (!parentname.compare("action")) {
                    m_tact.name = lastelt.data;
                    trimstring(m_tact.name);
                } else if (!parentname.compare("stateVariable")) {
                    m_tvar.name = lastelt.data;
                    trimstring(m_tvar.name);
                }
            }
            break;
        case 'r':
            if (!strcmp(name, "relatedStateVariable")) {
                m_targ.relatedVariable = lastelt.data;
                trimstring(m_targ.relatedVariable);
            } 
            break;
        case 's':
            if (!strcmp(name, "stateVariable")) {
                m_parsed.stateTable[m_tvar.name] = m_tvar;
            } else if (!strcmp(name, "step")) {
                m_tvar.hasValueRange = true;
                m_tvar.step = atoi(lastelt.data.c_str());
            }
            break;
        }

        m_path.pop_back();
    }

    virtual void CharacterData(const XML_Char *s, int len)
    {
        if (s == 0 || *s == 0)
            return;
        m_path.back().data += string(s, len);
    }

private:
    vector<StackEl> m_path;
    UPnPServiceDesc::Parsed& m_parsed;
    UPnPServiceDesc::Argument m_targ;
    UPnPServiceDesc::Action m_tact;
    UPnPServiceDesc::StateVariable m_tvar;
};

bool UPnPServiceDesc::fetchAndParseDesc(const string& urlbase, 
                                        Parsed& parsed) const
{
    char *buf = 0;
    char contentType[LINE_SIZE];
    string url = caturl(urlbase, SCPDURL);
    int code = UpnpDownloadUrlItem(url.c_str(), &buf, contentType);
    if (code != UPNP_E_SUCCESS) {
        LOGERR("UPnPServiceDesc::fetchAndParseDesc: error fetching " << 
               url << " : " << LibUPnP::errAsString("", code) << endl);
        return false;
    }
    string sdesc(buf);
    free(buf);
    ServiceDescriptionParser parser(parsed, sdesc);
    return parser.Parse();
}

} // namespace
