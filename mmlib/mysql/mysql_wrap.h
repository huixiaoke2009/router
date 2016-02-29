#ifndef _MYSQL_HPP_
#define _MYSQL_HPP_

#include <mysql/mysql.h>
#include <stdlib.h>

namespace mmlib
{

typedef struct{
    char sHost[100];
    char sUser[100];
    char sPasswd[100];
    char sDB[100];
    unsigned int nPort;
    char sTableName[100];
}stMySQL;

typedef int (ProcessBatchRecord)(MYSQL_ROW lRow);//<, char *lsBuffer, int nBuffSize)

class CMySQL
{

public:
    CMySQL ();
    ~CMySQL ();

    int Connect (char *sDBHost, char *sDBUser, char *sDBPasswd, char *sDBName=NULL, unsigned int nPort=3306);

    int UseDB (char *sDBName);


    //除查询外
    int Query (char *sSQL, int nSQLLen);
    //影响的行数
    unsigned long long AffectedRows();


    int GetRecordNum();

    int IsEnd();
    MYSQL_ROW GetFirst();
    MYSQL_ROW GetNext();

    void GetErrMsg (char *sErrMsg, int nMsgSize);
    char * GetErrMsg ();

    int Disconnect();

    //<int FetchRecord (int *nCount);
    //查询使用
    int Query (char *sSQL, int nSQLLen, int *nCount);
    MYSQL_ROW FetchRecord ();
    unsigned long *FetchLength ();

    int ReleaseRes();

    MYSQL_ROW FetchLocateRecord (int *nCount, int nLocate=0);

    int FetchBatchRecord (int *nRealCount, int nCount, ProcessBatchRecord *pFun);

    void SetAutoCommit( int nAutoCommit=1/* 0/1 */);
    int GetAutoCommit () { return m_nAutoCommit; }

    char *GetDB() { return m_sDBName; }

    //< return 0 when valid .
    int IsValid ();

    //<
    int EscapeString (char *sTo, const char *sFrom, int nFromLen);

    int FieldCount();

private:
    void SetErrMsg(const char *sErrMsg=NULL);

protected:
    char m_sErrMsg[200];
private:
    MYSQL m_MYSQL;
    MYSQL_RES *m_pRes;

    int m_nAutoCommit; //< 0/1

    int m_bDestroy;//< 0/1.
    char m_sDBName[50];

    //< auto reconncect.
    int m_nReConnect;//<0/1
    stMySQL m_MySQLInfo;
    bool m_bInit;
};

}
#endif
