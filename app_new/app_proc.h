
#ifndef _APP_PROC_H_
#define _APP_PROC_H_

#include <stdint.h>
#include <string>
#include <string.h>
#include "shm_queue/shm_queue.h"
#include "common.h"
#include "proxy.h"

class CApp
{
    public:
        CApp();
        ~CApp();

        int Init(const char *pConfFile);
        int Run();

    private:
        int m_DstID;

        CProxy proxy;
};


#endif
