#include <stdio.h>
#include "router_proc.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("usage: %s <conf_file>\n", argv[0]);
        return -1;
    }

    daemon(1, 1);

    int Ret = 0;
    CRouter Router;

    Ret = Router.Init(argv[1]);
    if (Ret != 0)
    {
        printf("ERR:proc init failed, ret=%d\n", Ret);
        return -1;
    }

    Ret = Router.Run();

    return Ret;
}

