#include "amflog.h"
#include <sys/types.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>

Log* Log::global_log = NULL;

int Log::write_log (int lvl , const char *pchFile , int iLine , const char* fmt, ...)
{
    if (lvl >= level)
        return 0;

    gettimeofday(&stLogTv, NULL);
    localtime_r(&stLogTv.tv_sec, &tm);

    char log_buf [4096];

    if (tm.tm_mday != log_day)
    {
        close_fd();
        open_fd();
    }
    
    int pos = 0;
    switch(lvl){
        case LVL_ERROR:
            pos = snprintf(log_buf, sizeof(log_buf), "[%02d:%02d:%02d.%.6lu][%s:%04d]<E> ",
                    tm.tm_hour, tm.tm_min, tm.tm_sec,    (unsigned long)stLogTv.tv_usec, pchFile, iLine);
            break;
        case LVL_STATS:
            pos = snprintf(log_buf, sizeof(log_buf), "[%02d:%02d:%02d.%.6lu]<S> ",
                    tm.tm_hour, tm.tm_min, tm.tm_sec,    (unsigned long)stLogTv.tv_usec);
            break;
        case LVL_INFO:            
            pos = snprintf(log_buf, sizeof(log_buf), "[%02d:%02d:%02d.%.6lu]<I> ",
                    tm.tm_hour, tm.tm_min, tm.tm_sec,    (unsigned long)stLogTv.tv_usec);
            break;
        case LVL_DEBUG:
            pos = snprintf(log_buf, sizeof(log_buf), "[%02d:%02d:%02d.%.6lu]<D> ",
                    tm.tm_hour, tm.tm_min, tm.tm_sec,    (unsigned long)stLogTv.tv_usec);
            break;
        case LVL_TRACE:
            pos = snprintf(log_buf, sizeof(log_buf), "[%02d:%02d:%02d.%.6lu][%s:%04d]<T> ",
                    tm.tm_hour, tm.tm_min, tm.tm_sec,    (unsigned long)stLogTv.tv_usec, pchFile, iLine);
            break;
        default:
            break;
    }    
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_buf + pos, sizeof(log_buf) - pos - 1, fmt, ap);
    va_end(ap);

    pos = strlen (log_buf);
    log_buf [pos] = '\n';
    log_buf [pos + 1] = 0;

    write (fd, log_buf, pos + 1);
    return     shift_fd();
}

void Log::init_log (const char* dir,int lvl, const char* pre_name,  int iLogSize)
{
    if(fd > 0)
        close_fd();
    memset(path, 0, sizeof(path));
    memset(prefix, 0, sizeof(prefix));
    snprintf(prefix, sizeof(prefix), "%s/%s", dir, pre_name);
    level = lvl;
    log_size = iLogSize;
    gettimeofday(&stLogTv, NULL);
    localtime_r(&stLogTv.tv_sec, &tm);
    open_fd();
}


void Log::open_fd ()
{
    snprintf(path, sizeof(path), "%s_%04d%02d%02d.log",
            prefix, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    fd = open (path, O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
    if (fd == -1)
        fprintf (stderr, "open %s error:%s\n", path, strerror(errno));
    log_day = tm.tm_mday;
}

void Log::close_fd ()
{
    close(fd);
    fd = -1;
}

int Log::shift_fd()
{
    static char sNewFile[512];
/* In write_log()
    if (tm.tm_mday != log_day)
    {
        close_fd();
        open_fd();
        return 0;
    }
*/
    struct stat stStat;

    if (stat(path, &stStat) < 0)
        return -1;

    if (stStat.st_size < log_size)
        return 0;
    else
    {
        close_fd();
        snprintf(sNewFile, sizeof(sNewFile), "%s.%02d%02d%02d",
             path, tm.tm_hour, tm.tm_min, tm.tm_sec);
        if(stat(sNewFile,&stStat) ==0)
            //File already existed....
        {
            snprintf(sNewFile, sizeof(sNewFile), "%s.%02d%02d%02d.%.6lu",
             path, tm.tm_hour, tm.tm_min, tm.tm_sec, (unsigned long)stLogTv.tv_usec);
        }

        if (rename(path,sNewFile) < 0 )
            {
                return -2;
            }
        open_fd();
    }
    return 0;
}
/*
int main()
{
    Log::instance()->init_log (".", 7, "conn");
    while(1)
    {
        for(int i =0; i < 200000; i++)
        {
            LOG(3,__FILE__,__LINE__,"Hi%d",i);
        }
    }
    return 0;
}
*/
