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
#ifndef _LOG_H_X_INCLUDED_
#define _LOG_H_X_INCLUDED_

#include <fstream>                      // for ofstream
#include <iostream>                     // for cerr, ostream
#include <string>                       // for string

namespace UPnPP {

    class Logger {
    public:
        static Logger *getTheLog(const std::string& fn);
        std::ostream& getstream() {
            return m_tocerr ? std::cerr : m_stream;
        }
        enum LogLevel {LLNON, LLFAT, LLERR, LLINF, LLDEB, LLDEB1};
        void setLogLevel(LogLevel level) {
            m_loglevel = level;
        }
        int getloglevel() {
            return m_loglevel;
        }
        
    private:
        bool m_tocerr;
        int m_loglevel;
        std::ofstream m_stream;
        
        Logger(const std::string& fn);
	Logger(const Logger &);
	Logger& operator=(const Logger &);
    };
        
#define DEBOUT (Logger::getTheLog("")->getstream())
#ifndef LOCAL_LOGINC
#define LOCAL_LOGINC 0
#endif
#define LOGLEVEL (Logger::getTheLog("")->getloglevel()+LOCAL_LOGINC)

#define LOGDEB1(X) {                                                    \
        if (LOGLEVEL >= Logger::LLDEB1)                     \
        {                                                               \
            DEBOUT << __FILE__ << ":" << __LINE__<< "::"; DEBOUT << X;  \
        }                                                               \
    }

#define LOGDEB(X) {                                                     \
        if (LOGLEVEL >= Logger::LLDEB)                      \
        {                                                               \
            DEBOUT << __FILE__ << ":" << __LINE__<< "::"; DEBOUT << X;  \
        }                                                               \
    }

#define LOGINF(X) {                                                     \
        if (LOGLEVEL >= Logger::LLINF)                      \
        {                                                               \
            DEBOUT << __FILE__ << ":" << __LINE__<< "::"; DEBOUT << X;  \
        }                                                               \
    }                                                                   

#define LOGERR(X) {                                                     \
        if (LOGLEVEL >= Logger::LLERR)                      \
        {                                                               \
            DEBOUT << __FILE__ << ":" << __LINE__<< "::"; DEBOUT << X;  \
        }                                                               \
    }

#define LOGFAT(X) {                                                     \
        if (LOGLEVEL >= Logger::LLFAT)                      \
        {                                                               \
            DEBOUT << __FILE__ << ":" << __LINE__<< "::"; DEBOUT << X;  \
        }                                                               \
    }

} // namespace UPnPP


#endif /* _LOG_H_X_INCLUDED_ */
