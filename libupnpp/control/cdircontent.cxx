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

#include "libupnpp/control/cdircontent.hxx"

#include <expat.h>                      // for XML_GetCurrentByteIndex
#include <expat_external.h>             // for XML_Char, XML_Size
#include <string.h>                     // for strcmp

#include <string>                       // for string, allocator, etc
#include <unordered_map>                // for unordered_map, etc
#include <vector>                       // for vector

#include "libupnpp/expatmm.hxx"         // for inputRefXMLParser
#include "libupnpp/log.hxx"             // for LOGINF
#include "libupnpp/upnpp_p.hxx"         // for trimstring

using namespace std;
using namespace UPnPP;

namespace UPnPClient {

// An XML parser which builds directory contents from DIDL-lite input.
class UPnPDirParser : public inputRefXMLParser {
public:
    UPnPDirParser(UPnPDirContent& dir, const string& input)
        : inputRefXMLParser(input), m_dir(dir)
    {
        //LOGDEB("UPnPDirParser: input: " << input << endl);
        m_okitems["object.item.audioItem.musicTrack"] =
            UPnPDirObject::audioItem_musicTrack;
        m_okitems["object.item.playlistItem"] =
            UPnPDirObject::audioItem_playlist;
    }
    UPnPDirContent& m_dir;

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
        m_path.back().sta = XML_GetCurrentByteIndex(expat_parser);
        for (int i = 0; attrs[i] != 0; i += 2) {
            m_path.back().attributes[attrs[i]] = attrs[i+1];
        }

        switch (name[0]) {
        case 'c':
            if (!strcmp(name, "container")) {
                m_tobj.clear();
                m_tobj.m_type = UPnPDirObject::container;
                m_tobj.m_id = m_path.back().attributes["id"];
                m_tobj.m_pid = m_path.back().attributes["parentID"];
            }
            break;
        case 'i':
            if (!strcmp(name, "item")) {
                m_tobj.clear();
                m_tobj.m_type = UPnPDirObject::item;
                m_tobj.m_id = m_path.back().attributes["id"];
                m_tobj.m_pid = m_path.back().attributes["parentID"];
            }
            break;
        default:
            break;
        }
    }

    virtual bool checkobjok()
    {
        // I don't see the point of this checking any more. Let the caller deal
        // with filtering entries.
        return true;

        bool ok =  !m_tobj.m_id.empty() && !m_tobj.m_pid.empty() &&
            !m_tobj.m_title.empty();

        if (ok && m_tobj.m_type == UPnPDirObject::item) {
            map<string, UPnPDirObject::ItemClass>::const_iterator it;
            it = m_okitems.find(m_tobj.m_props["upnp:class"]);
            if (it == m_okitems.end()) {
                LOGINF("checkobjok: found object of unknown class: [" <<
                       m_tobj.m_props["upnp:class"] << endl);
                ok = false;
            } else {
                m_tobj.m_iclass = it->second;
            }
        }

        if (!ok) {
            LOGINF("checkobjok:skip: id ["<< m_tobj.m_id<<"] pid ["<<
                   m_tobj.m_pid << "] clss [" << m_tobj.m_props["upnp:class"]
                   << "] tt [" << m_tobj.m_title << "]" << endl);
        }
        return ok;
    }

    virtual void EndElement(const XML_Char *name)
    {
        string parentname;
        if (m_path.size() == 1) {
            parentname = "root";
        } else {
            parentname = m_path[m_path.size()-2].name;
        }
        //LOGDEB("Closing element " << name << " inside element " << 
        //       parentname << " data " << m_path.back().data << endl);
        if (!strcmp(name, "container")) {
            if (checkobjok()) {
                m_dir.m_containers.push_back(m_tobj);
            }
        } else if (!strcmp(name, "item")) {
            if (checkobjok()) {
                unsigned int len = XML_GetCurrentByteIndex(expat_parser) - 
                    m_path.back().sta;
                m_tobj.m_didlfrag = m_input.substr(m_path.back().sta, len)
                    + "</item></DIDL-Lite>";
                m_dir.m_items.push_back(m_tobj);
            }
        } else if (!parentname.compare("item") || 
                   !parentname.compare("container")) {
            switch (name[0]) {
            case 'd':
                if (!strcmp(name, "dc:title"))
                    m_tobj.m_title = m_path.back().data;
                break;
            case 'r':
                if (!strcmp(name, "res")) {
                    // <res protocolInfo="http-get:*:audio/mpeg:*" size="517149"
                    // bitrate="24576" duration="00:03:35" 
                    // sampleFrequency="44100" nrAudioChannels="2">
                    UPnPResource res;
                    res.m_uri = m_path.back().data;
                    for (auto it =  m_path.back().attributes.begin();
                        it !=  m_path.back().attributes.end(); it++) {
                        res.m_props[it->first] = it->second;
                    }
                    m_tobj.m_resources.push_back(res);
                }
                break;
            default:
                m_tobj.m_props[name] = m_path.back().data;
                break;
            }

        }

        m_path.pop_back();
    }

    virtual void CharacterData(const XML_Char *s, int len)
    {
        if (s == 0 || *s == 0)
            return;
        string str(s, len);
        trimstring(str);
        m_path.back().data += str;
    }

private:
    vector<StackEl> m_path;
    UPnPDirObject m_tobj;
    map<string, UPnPDirObject::ItemClass> m_okitems;
};

bool UPnPDirContent::parse(const std::string& input)
{
    UPnPDirParser parser(*this, input);
    return parser.Parse();
}

static const string didl_header(
"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
"<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\""
" xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
" xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\""
" xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">");

// Maybe we'll do something about building didl from scratch if this
// proves necessary.
string UPnPDirObject::getdidl() const
{
    return didl_header + m_didlfrag;
}

} // namespace
