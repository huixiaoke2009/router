#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include "process_manager.h"

using namespace  mmlib;
CProcessManager::CProcessManager( unsigned int iChildMinNum, unsigned int iChildMaxNum )
{
    m_iChildMinNum        = iChildMinNum;
    m_iChildMaxNum        = iChildMaxNum;
}

int CProcessManager::SetChildNum(unsigned int iChildMinNum , unsigned int iChildMaxNum)
{
    m_iChildMinNum      = iChildMinNum;
    m_iChildMaxNum      = iChildMaxNum;
    return 0;
}

CProcessManager::~CProcessManager()
{
}

int CProcessManager::Run( int argc, char *argv[] )
{
    //create process entity
    for ( int i = 0; i < m_iChildMinNum; )
    {
        int iPid = fork();
        if ( iPid < 0 )
        {
            printf( "ERR: CProcessManager::Run() fork failed! ERRMSG:%s\n", strerror( errno ) );
            continue;
        }
        else if ( iPid > 0 )
        {
            i++;
        }
        else if ( iPid == 0 )
        {
            int iRet = Entity( argc, argv );
            if ( iRet < 0 )
            {
                printf( "ERR: CProcessManager::Entity() return < 0. RetValue = %d\n", iRet );
            }

            _exit( iRet );
        }
    }

    while(1)
    {
        //monitor all child processes
        int iStatus = 0;
        int iPid = waitpid( -1, &iStatus, WUNTRACED );
        if ( WIFEXITED( iStatus ) != 0 )
        {
            printf( "NOTICE:PID: %d exited normally!\n", iPid );
        }
        else if ( WIFSIGNALED( iStatus ) != 0 )
        {
            printf( "NOTICE:PID: %d exited bacause of signal ID: [%d] has not been catched!\n", iPid, WTERMSIG( iStatus ) );
        }
        else if ( WIFSTOPPED( iStatus ) != 0 )
        {
            printf( "NOTICE:PID: %d exited because of stoped! ID: [%d]\n", iPid, WSTOPSIG( iStatus ) );
            
        }
        else
        {
            printf( "NOTICE:PID: %d exited abnormally!\n", iPid );
        }

        //Add Entity
        AddEntity( argc, argv );
    }

    return SUCCESS;
}

int CProcessManager::Entity( int argc, char *argv[] )
{
    while( 1 )
    {
        printf( "PID[%d]----I am live----%ld-----\n", getpid(), time(NULL) );
        sleep( 5 );
    }
    
    return SUCCESS;
}

int CProcessManager::AddEntity( int argc, char *argv[] )
{
    int iPid = fork();
    if ( iPid < 0 )
    {
        printf( "ERR: CProcessManager::Run() fork failed! ERRMSG:%s\n", strerror( errno ) );
        return ERROR;
    }
    else if ( iPid == 0 )
    {
        int iRet = Entity( argc, argv );
        if ( iRet < 0 )
        {
            printf( "ERR: CProcessManager::Entity() return < 0. RetValue = %d\n", iRet );
        }

        printf( "NOTICE:Add Entity: %d\n", getpid() );

        _exit( iRet );
    }

    return SUCCESS;
}
