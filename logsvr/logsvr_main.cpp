#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logsvr_proc.h"
#include "util/util.h"

int main(int argc, char *argv[])
{
    int iRetVal = 0;

    if (argc < 2)
    {
        printf("usage: %s <conf_file>\n", argv[0]);
        return -1;
    }

    char *pszConfFile = argv[1];
    char LogsvrPidName[64] = {0};
    sprintf(LogsvrPidName, "logsvr");

    mmlib::CSysTool::DaemonInit();
    if(mmlib::CSysTool::LockProc(LogsvrPidName) != 0)
    {
        printf("%s have started\n", argv[0]);
        return -1;
    }

    CLogSvrProc objLogSvrProc;

    iRetVal = objLogSvrProc.Init(pszConfFile);
    if (iRetVal != objLogSvrProc.SUCCESS)
    {
        printf("init logsvr proc failed, ret=%d\n", iRetVal);
        return -2;
    }

    iRetVal = objLogSvrProc.Run();
    if (iRetVal != objLogSvrProc.SUCCESS)
    {
        printf("run logsvr proc failed, ret=%d\n", iRetVal);
        return -3;
    }

    return 0;
}

