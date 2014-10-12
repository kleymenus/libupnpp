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

#include "libupnpp/ixmlwrap.hxx"

#include <upnp/ixml.h>                  // for IXML_Node, etc

#include <string>                       // for string

using std::string;

namespace UPnPP {

#if notUsedAnyMore
// Get the value for the first element in the document with the given name.
// There should be only one such element for this to make any sense.
string getFirstElementValue(IXML_Document *doc, const string& name)
{
    string ret;
    IXML_NodeList *nodes =
        ixmlDocument_getElementsByTagName(doc, name.c_str());

    if (nodes) {
        IXML_Node *first = ixmlNodeList_item(nodes, 0);
        if (first) {
            IXML_Node *dnode = ixmlNode_getFirstChild(first);
            if (dnode) {
                ret = ixmlNode_getNodeValue(dnode);
            }
        }
    }

    if(nodes)
        ixmlNodeList_free(nodes);
    return ret;
}
#endif

string ixmlwPrintDoc(IXML_Document* doc)
{
    DOMString s = ixmlPrintDocument(doc);
    if (s) {
        string cpps(s);
        ixmlFreeDOMString(s);
        return cpps;
    } else {
        return string();
    }
}

}
