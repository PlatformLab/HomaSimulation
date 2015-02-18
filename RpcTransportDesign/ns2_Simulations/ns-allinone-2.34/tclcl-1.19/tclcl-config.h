#ifndef TCLCL_CONFIG_H
#define TCLCL_CONFIG_H

#ifdef WIN32

#include <windows.h>
#include <winsock.h>
typedef unsigned int UINT;

#ifndef TIMEZONE_DEFINED_
#define TIMEZONE_DEFINED_
struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};
#endif 
    
#if defined(__cplusplus)
extern "C" {
#endif
        int gettimeofday(struct timeval *tp, struct timezone *tz);
        void displayErr(const char* prefix);

#if defined(__cplusplus)
} /* end extern "C" */
#endif

#endif /* WIN32 */

// 64-bit integer support
#if defined(SIZEOF_LONG) && SIZEOF_LONG >= 8
#define STRTOI64 strtol
#define STRTOI64_FMTSTR "%ld"

#elif defined(HAVE_STRTOQ)
#define STRTOI64 strtoq
#define STRTOI64_FMTSTR "%lld"

#elif defined(HAVE_STRTOLL)
#define STRTOI64 strtoll
#define STRTOI64_FMTSTR "%lld"
#endif

#endif /*TCLCL_CONFIG_H*/
