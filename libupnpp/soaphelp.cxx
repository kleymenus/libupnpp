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

#include "libupnpp/soaphelp.hxx"

#include <stdio.h>                      // for sprintf
#include <stdlib.h>                     // for atoi

#include <iostream>                     // for operator<<, endl, etc

#include "libupnpp/log.hxx"             // for LOGDEB, LOGERR, LOGDEB1
#include "libupnpp/upnpp_p.hxx"         // for stringToBool

using namespace std;

namespace UPnPP {

class SoapIncoming::Internal {
public:
    std::string name;
    std::map<std::string, std::string> args;
};

SoapIncoming::SoapIncoming() 
{
    if ((m = new Internal()) == 0) {
        LOGERR("SoapIncoming::SoapIncoming: out of memory" << endl);
        return;
    }
}
SoapIncoming::~SoapIncoming() 
{
    delete m;
    m = 0;
}

/* Example Soap XML doc passed by libupnp is like: 
   <ns0:SetMute>
     <InstanceID>0</InstanceID>
     <Channel>Master</Channel>
     <DesiredMute>False</DesiredMute>
   </ns0:SetMute>
   
   As the top node name is qualified by a namespace, it's easier to just use 
   action name passed in the libupnp action callback.
   
   This is used both for decoding action requests in the device and responses
   in the control point side
*/
bool SoapIncoming::decode(const char *callnm, IXML_Document *actReq)
{
    m->name = callnm;

    IXML_NodeList* nl = 0;
    IXML_Node* topNode = ixmlNode_getFirstChild((IXML_Node *)actReq);
    if (topNode == 0) {
        LOGERR("SoapIncoming: Empty Action request (no topNode) ??" << endl);
        return false;
    }
    //LOGDEB("SoapIncoming: top node name: " << ixmlNode_getNodeName(topNode) 
    //       << endl);

    nl = ixmlNode_getChildNodes(topNode);
    if (nl == 0) {
        // Ok actually, there are no args
        return true;
    }
    //LOGDEB("SoapIncoming: childnodes list length: " << ixmlNodeList_length(nl)
    // << endl);
    bool ret = false;
    for (unsigned long i = 0; i <  ixmlNodeList_length(nl); i++) {
        IXML_Node *cld = ixmlNodeList_item(nl, i);
        if (cld == 0) {
            //LOGDEB1("SoapIncoming: got null node  from nodelist at index " <<
            // i << " ??" << endl);
            // Seems to happen with empty arg list?? This looks like a bug, 
            // should we not get an empty node instead?
            if (i == 0) {
                ret = true;
            }
            goto out;
        }
        const char *name = ixmlNode_getNodeName(cld);
        if (name == 0) {
            DOMString pnode = ixmlPrintNode(cld);
            LOGDEB("SoapIncoming: got null name ??:" << pnode << endl);
            ixmlFreeDOMString(pnode);
            goto out;
        }
        IXML_Node *txtnode = ixmlNode_getFirstChild(cld);
        const char *value = "";
        if (txtnode != 0) {
            value = ixmlNode_getNodeValue(txtnode);
        }
        // Can we get an empty value here ?
        if (value == 0)
            value = "";
        m->args[name] = value;
    }
    m->name = callnm;
    ret = true;

out:
    if (nl)
        ixmlNodeList_free(nl);
    return ret;
}

const std::string& SoapIncoming::getName() const 
{
    return m->name;
}

bool SoapIncoming::get(const char *nm, bool *value) const
{
    map<string, string>::const_iterator it = m->args.find(nm);
    if (it == m->args.end() || it->second.empty()) {
        return false;
    }
    return stringToBool(it->second, value);
}

bool SoapIncoming::get(const char *nm, int *value) const
{
    map<string, string>::const_iterator it = m->args.find(nm);
    if (it == m->args.end() || it->second.empty()) {
        return false;
    }
    *value = atoi(it->second.c_str());
    return true;
}

bool SoapIncoming::get(const char *nm, string *value) const
{
    map<string, string>::const_iterator it = m->args.find(nm);
    if (it == m->args.end()) {
        return false;
    }
    *value = it->second;
    return true;
}

string SoapHelp::xmlQuote(const string& in)
{
    string out;
    for (unsigned int i = 0; i < in.size(); i++) {
        switch(in[i]) {
        case '"': out += "&quot;";break;
        case '&': out += "&amp;";break;
        case '<': out += "&lt;";break;
        case '>': out += "&gt;";break;
        case '\'': out += "&apos;";break;
        default: out += in[i];
        }
    }
    return out;
}

string SoapHelp::xmlUnquote(const string& in)
{
    string out;
    for (unsigned int i = 0; i < in.size(); i++) {
        if (in[i] == '&') {
            unsigned int j;
            for (j = i; j < in.size(); j++) {
                if (in[j] == ';')
                    break;
            }
            if (in[j] != ';') {
                out += in.substr(i);
                return out;
            }
            string entname = in.substr(i+1, j-i-1);
            //cerr << "entname [" << entname << "]" << endl;
            if (!entname.compare("quot")) {
                out += '"';
            } else if (!entname.compare("amp")) {
                out += '&';
            } else if (!entname.compare("lt")) {
                out += '<';
            } else if (!entname.compare("gt")) {
                out += '>';
            } else if (!entname.compare("apos")) {
                out += '\'';
            } else {
                out += in.substr(i, j-i+1);
            }
            i = j;
        } else {
            out += in[i];
        }
    }
    return out;
}

// Yes inefficient. whatever...
string SoapHelp::i2s(int val)
{
    char cbuf[30];
    sprintf(cbuf, "%d", val);
    return string(cbuf);
}

class SoapOutgoing::Internal {
public:
    std::string serviceType;
    std::string name;
    std::vector<std::pair<std::string, std::string> > data;
};

SoapOutgoing::SoapOutgoing() 
{
    if ((m = new Internal()) == 0) {
        LOGERR("SoapOutgoing::SoapOutgoing: out of memory" << endl);
        return;
    }
}

SoapOutgoing::SoapOutgoing(const std::string& st, const std::string& nm)
{
    if ((m = new Internal()) == 0) {
        LOGERR("SoapOutgoing::SoapOutgoing: out of memory" << endl);
        return;
    }
    m->serviceType = st;
    m->name = nm;
}

SoapOutgoing::~SoapOutgoing()
{
    delete m;
    m = 0;
}

const string& SoapOutgoing::getName() const 
{
    return m->name;
}

SoapOutgoing& SoapOutgoing::addarg(const string& k, const string& v) 
{
    m->data.push_back(pair<string, string>(k, v));
    return *this;
}

SoapOutgoing& SoapOutgoing::operator() (const string& k, const string& v) 
{
    m->data.push_back(pair<string, string>(k, v));
    return *this;
}

IXML_Document *SoapOutgoing::buildSoapBody(bool isResponse) const
{
    IXML_Document *doc = ixmlDocument_createDocument();
    if (doc == 0) {
        cerr << "buildSoapBody: out of memory" << endl;
        return 0;
    }
    string topname = string("u:") + m->name;
    if (isResponse)
        topname += "Response";

    IXML_Element *top =  
        ixmlDocument_createElementNS(doc, m->serviceType.c_str(), 
                                     topname.c_str());
    ixmlElement_setAttribute(top, "xmlns:u", m->serviceType.c_str());

    for (unsigned i = 0; i < m->data.size(); i++) {
        IXML_Element *elt = 
            ixmlDocument_createElement(doc, m->data[i].first.c_str());
        IXML_Node* textnode = 
            ixmlDocument_createTextNode(doc, m->data[i].second.c_str());
        ixmlNode_appendChild((IXML_Node*)elt,(IXML_Node*)textnode);
        ixmlNode_appendChild((IXML_Node*)top,(IXML_Node*)elt);
    }

    ixmlNode_appendChild((IXML_Node*)doc,(IXML_Node*)top);
    
    return doc;
}

// Decoding UPnP Event data. The variable values are contained in a
// propertyset XML document:
//     <?xml version="1.0"?>
//     <e:propertyset xmlns:e="urn:schemas-upnp-org:event-1-0">
//       <e:property>
//         <variableName>new value</variableName>
//       </e:property>
//       <!-- Other variable names and values (if any) go here. -->
//     </e:propertyset>

bool decodePropertySet(IXML_Document *doc, 
                       unordered_map<string,string>& out)
{
    bool ret = false;
    IXML_Node* topNode = ixmlNode_getFirstChild((IXML_Node *)doc);
    if (topNode == 0) {
        LOGERR("decodePropertySet: (no topNode) ??" << endl);
        return false;
    }
    //LOGDEB("decodePropertySet: topnode name: " << 
    //       ixmlNode_getNodeName(topNode) << endl);

    IXML_NodeList* nl = ixmlNode_getChildNodes(topNode);
    if (nl == 0) {
        LOGDEB("decodePropertySet: empty list" << endl);
        return true;
    }
    for (unsigned long i = 0; i <  ixmlNodeList_length(nl); i++) {
        IXML_Node *cld = ixmlNodeList_item(nl, i);
        if (cld == 0) {
            LOGDEB("decodePropertySet: got null node  from nlist at index " <<
                   i << " ??" << endl);
            // Seems to happen with empty arg list?? This looks like a bug, 
            // should we not get an empty node instead?
            if (i == 0) {
                ret = true;
            }
            goto out;
        }
        const char *name = ixmlNode_getNodeName(cld);
        //LOGDEB("decodePropertySet: got node name:     " << 
        //   ixmlNode_getNodeName(cld) << endl);
        if (cld == 0) {
            DOMString pnode = ixmlPrintNode(cld);
            //LOGDEB("decodePropertySet: got null name ??:" << pnode << endl);
            ixmlFreeDOMString(pnode);
            goto out;
        }
        IXML_Node *subnode = ixmlNode_getFirstChild(cld);
        name = ixmlNode_getNodeName(subnode);
        //LOGDEB("decodePropertySet: got subnode name:         " << 
        //   name << endl);
        
        IXML_Node *txtnode = ixmlNode_getFirstChild(subnode);
        //LOGDEB("decodePropertySet: got txtnode name:             " << 
        //   ixmlNode_getNodeName(txtnode) << endl);
        
        const char *value = "";
        if (txtnode != 0) {
            value = ixmlNode_getNodeValue(txtnode);
        }
        // Can we get an empty value here ?
        if (value == 0)
            value = "";
        // ixml does the unquoting. Don't call xmlUnquote here
        out[name] = value; 
    }

    ret = true;
out:
    if (nl)
        ixmlNodeList_free(nl);
    return ret;
}

} // namespace
