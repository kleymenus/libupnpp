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
#ifndef _OHPLAYLIST_HXX_INCLUDED_
#define _OHPLAYLIST_HXX_INCLUDED_

#include <memory>                       // for shared_ptr
#include <string>                       // for string
#include <unordered_map>                // for unordered_map
#include <vector>                       // for vector

#include "cdircontent.hxx"              // for UPnPDirObject
#include "service.hxx"                  // for Service

namespace UPnPClient { class OHPlaylist; }
namespace UPnPClient { class UPnPDeviceDesc; }
namespace UPnPClient { class UPnPServiceDesc; }

namespace UPnPClient {

typedef std::shared_ptr<OHPlaylist> OHPLH;

/**
 * OHPlaylist Service client class.
 *
 */
class OHPlaylist : public Service {
public:

    OHPlaylist(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
        : Service(device, service) {
        registerCallback();
    }
    virtual ~OHPlaylist() {
        unregisterCallback();
    }

    OHPlaylist() {}

    /** Test service type from discovery message */
    static bool isOHPlService(const std::string& st);

    int play();
    int pause();
    int stop();
    int next();
    int previous();
    int setRepeat(bool onoff);
    int repeat(bool *on);
    int setShuffle(bool onoff);
    int shuffle(bool *on);
    int seekSecondAbsolute(int value);
    int seekSecondRelative(int value);
    int seekId(int value);
    int seekIndex(int value);
    enum TPState {TPS_Unknown, TPS_Buffering, TPS_Paused, TPS_Playing,
                  TPS_Stopped};
    int transportState(TPState *tps);
    int id(int *value);
    int read(int id, std::string* uri, UPnPDirObject *dirent);

    struct TrackListEntry {
        int id;
        std::string url;
        UPnPDirObject dirent;
        void clear() {id = -1; url.clear(); dirent.clear();}
    };
    int readList(const std::vector<int>& ids, 
                 std::vector<TrackListEntry>* entsp);

    int insert(int afterid, const std::string& uri, const std::string& didl, 
               int *nid);
    int deleteId(int id);
    int deleteAll();
    int tracksMax(int *);
    int idArray(std::vector<int> *ids, int *tokp);
    int idArrayChanged(int token, bool *changed);
    int protocolInfo(std::string *proto);

protected:
    /* My service type string */
    static const std::string SType;

private:
    void evtCallback(const std::unordered_map<std::string, std::string>&);
    void registerCallback();
};

} // namespace UPnPClient

#endif /* _OHPLAYLIST_HXX_INCLUDED_ */
