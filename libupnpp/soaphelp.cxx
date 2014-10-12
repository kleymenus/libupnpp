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
bool decodeSoapBody(const char *callnm, IXML_Document *actReq, 
                    SoapDecodeOutput *res)
{
    bool ret = false;
    IXML_NodeList* nl = 0;
    IXML_Node* topNode = 
        ixmlNode_getFirstChild((IXML_Node *)actReq);
    if (topNode == 0) {
        LOGERR("decodeSoap: Empty Action request (no topNode) ??" << endl);
        return false;
    }
    //LOGDEB("decodeSoap: top node name: " << ixmlNode_getNodeName(topNode) 
    //       << endl);

    nl = ixmlNode_getChildNodes(topNode);
    if (nl == 0) {
        // Ok actually, there are no args
        return true;
    }
    //LOGDEB("decodeSoap: childnodes list length: " << ixmlNodeList_length(nl)
    // << endl);

    for (unsigned long i = 0; i <  ixmlNodeList_length(nl); i++) {
        IXML_Node *cld = ixmlNodeList_item(nl, i);
        if (cld == 0) {
            LOGDEB1("decodeSoap: got null node  from nodelist at index " <<
                   i << " ??" << endl);
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
            LOGDEB("decodeSoap: got null name ??:" << pnode << endl);
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
        res->args[name] = value;
    }
    res->name = callnm;
    ret = true;
out:
    if (nl)
        ixmlNodeList_free(nl);
    return ret;
}

bool SoapDecodeOutput::getBool(const char *nm, bool *value) const
{
    map<string, string>::const_iterator it = args.find(nm);
    if (it == args.end() || it->second.empty()) {
        return false;
    }
    return stringToBool(it->second, value);
}

bool SoapDecodeOutput::getInt(const char *nm, int *value) const
{
    map<string, string>::const_iterator it = args.find(nm);
    if (it == args.end() || it->second.empty()) {
        return false;
    }
    *value = atoi(it->second.c_str());
    return true;
}

bool SoapDecodeOutput::getString(const char *nm, string *value) const
{
    map<string, string>::const_iterator it = args.find(nm);
    if (it == args.end()) {
        return false;
    }
    *value = it->second;
    return true;
}

namespace SoapHelp {
string xmlQuote(const string& in)
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

string xmlUnquote(const string& in)
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
string i2s(int val)
{
    char cbuf[30];
    sprintf(cbuf, "%d", val);
    return string(cbuf);
}

}

IXML_Document *buildSoapBody(const SoapEncodeInput& data, bool isResponse)
{
    IXML_Document *doc = ixmlDocument_createDocument();
    if (doc == 0) {
        cerr << "buildSoapResponse: out of memory" << endl;
        return 0;
    }
    string topname = string("u:") + data.name;
    if (isResponse)
        topname += "Response";

    IXML_Element *top =  
        ixmlDocument_createElementNS(doc, data.serviceType.c_str(), 
                                     topname.c_str());
    ixmlElement_setAttribute(top, "xmlns:u", data.serviceType.c_str());

    for (unsigned i = 0; i < data.data.size(); i++) {
        IXML_Element *elt = 
            ixmlDocument_createElement(doc, data.data[i].first.c_str());
        IXML_Node* textnode = 
            ixmlDocument_createTextNode(doc, data.data[i].second.c_str());
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
