#ifndef __PROCESS_MANAGER_H__
#define __PROCESS_MANAGER_H__
namespace mmlib
{
class CProcessManager
{
    /* -----------------------------------------------------------
    *  在processManager的世界里
    *                      0       表示成功
    *                    <0         表示系统异常
    *                    >0        程序正常返回值
    *  ---------------------------------------------------------*/
public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;

public:
    CProcessManager( unsigned int iChildMinNum = 16, unsigned int iChildMaxNum = 128 );
    int SetChildNum(unsigned int iChildMinNum , unsigned int iChildMaxNum);
    virtual ~CProcessManager();

    //开始Process Manager运行
    int Run( int argc, char *argv[] );

    //程序实体，实体异常推出或者需要新的处理实体时此函数会被调用
    virtual int Entity( int argc, char *argv[] );

    //补充运行实体，每次补充1个
    int AddEntity( int argc, char *argv[] );
    
private:
    int     m_iChildMinNum;
    int     m_iChildMaxNum;
};
}
#endif        //_PROCESS_MANAGER_H__
