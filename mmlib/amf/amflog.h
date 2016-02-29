#ifndef LOG_H
#define LOG_H
#include <string>
#include <sys/time.h> 
#include <time.h> 

#ifndef __FL__
#define __FL__  __FILE__, __LINE__
#endif

using namespace std;
#define LVL_ERROR    0
/* non-fatal errors */
#define LVL_STATS    1
#define LVL_INFO    2
/* suppressed unless DEBUG defined;*/
#define LVL_DEBUG    3
/* not shown by default */
#define LVL_TRACE    4

#define LOG_ERROR     0,__FILE__,__LINE__ 
#define LOG_STATS     1,__FILE__,__LINE__ 
#define LOG_INFO    2,__FILE__,__LINE__ 
#define LOG_DEBUG     3,__FILE__,__LINE__ 
#define LOG_TRACE    4,__FILE__,__LINE__ 

class Log
{
public:
    ~Log()
    {
        close_fd ();
    }
    static Log* instance ()
    {
        if (global_log == NULL)
            global_log = new Log;
        return global_log;
    }

    int write_log (int lvl , const char *pchFile, int iLine , const char* fmt, ...);
    void init_log (const char* dir , int lvl, const char* pre_name = NULL, int iLogSize = 100000000);
private:
    Log () {        fd = -1;    };
    static    Log* global_log;

    int        fd;
    int        log_day;
    int        level;
    int        log_num;
    int        log_size;
    char    path[512];
    char    prefix[256];

    struct timeval stLogTv;
    struct tm tm;

    void    open_fd ();
    void    close_fd ();
    int        shift_fd ();
};

extern Log* global_log;
#define LOG    Log::instance()->write_log
#endif
