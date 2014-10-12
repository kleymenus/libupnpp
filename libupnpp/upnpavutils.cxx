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
#include "libupnpp/upnpavutils.hxx"

#include <cstdio>
#include <string>

using namespace std;

namespace UPnPP {

// Format duration in milliseconds into UPnP duration format
string upnpduration(int ms)
{
    int hours = ms / (3600 * 1000);
    ms -= hours * 3600 * 1000;
    int minutes = ms / (60 * 1000);
    ms -= minutes * 60 * 1000;
    int secs = ms / 1000;
    ms -= secs * 1000;

    char cbuf[100];

    // This is the format from the ref doc, but it appears that the
    // decimal part in the seconds field is an issue with some control
    // points. So drop it...
    //  sprintf(cbuf, "%d:%02d:%02d.%03d", hours, minutes, secs, ms);
    sprintf(cbuf, "%d:%02d:%02d", hours, minutes, secs);
    return cbuf;
}

// H:M:S to seconds
int upnpdurationtos(const string& dur)
{
    int hours, minutes, seconds;
    if (sscanf(dur.c_str(), "%d:%d:%d", &hours, &minutes, &seconds) != 3) {
	return 0;
    }
    return 3600 * hours + 60 * minutes + seconds;
}

}
