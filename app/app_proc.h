
#ifndef _APP_PROC_H_
#define _APP_PROC_H_

#include <stdint.h>
#include <string>
#include <string.h>
#include "shm_queue/shm_queue.h"
#include "common.h"

class CApp
{
    public:
        CApp();
        ~CApp();

        int Init(const char *pConfFile);
        int Run();

    private:
        
    private:
        int m_SendFlag;
        mmlib::CShmQueue m_ProxyQueue;
        mmlib::CShmQueue m_ConnQueue;
};


#endif
