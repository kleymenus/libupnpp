#ifndef _HTTPDOWNLOAD_H_X_INCLUDED_
#define _HTTPDOWNLOAD_H_X_INCLUDED_

#include <string>

extern bool downloadUrlWithCurl(const std::string& url, 
                                std::string& out, long timeoutsecs);

#endif /* _HTTPDOWNLOAD.H_X_INCLUDED_ */
