#include <string.h>
#include <stdio.h>
#include "mysql/mysql_wrap.h"

using namespace mmlib;

CMySQL::CMySQL()
{
    m_nAutoCommit = 1;
    m_bInit = false;
    memset (m_sDBName, 0, sizeof(m_sDBName));
    memset (&m_MySQLInfo, 0, sizeof(m_MySQLInfo));
}

CMySQL::~CMySQL ()
{
    Disconnect();
}

int CMySQL::Connect(char *sDBHost, char *sDBUser, char *sDBPasswd, char *sDBName, unsigned int nPort)
{
    if ( mysql_init(&m_MYSQL) == NULL)
    {
        SetErrMsg ();
        return -1;
    }
    m_bInit = true;

    if ( mysql_real_connect(&m_MYSQL, sDBHost, sDBUser, sDBPasswd, sDBName, nPort, NULL, 0) == NULL)
    {
        SetErrMsg();
        mysql_close(&m_MYSQL);
        return -2;
    }

    if (sDBName != NULL)
    {
        strncpy (m_sDBName, sDBName, sizeof(m_sDBName));
        strncpy (m_MySQLInfo.sDB, sDBName, sizeof(m_MySQLInfo.sDB));
    }

    strncpy (m_MySQLInfo.sHost, sDBHost, sizeof(m_MySQLInfo.sHost));
    strncpy (m_MySQLInfo.sUser, sDBUser, sizeof(m_MySQLInfo.sUser));
    strncpy (m_MySQLInfo.sPasswd, sDBPasswd, sizeof(m_MySQLInfo.sPasswd));
    m_MySQLInfo.nPort = nPort;

    return 0;
}

int CMySQL::Disconnect()
{
    if (&m_MYSQL != NULL && m_bInit)
        mysql_close(&m_MYSQL);
    return 0;
}

void CMySQL::SetErrMsg (const char *sErrMsg)
{
    memset (m_sErrMsg, 0, sizeof(m_sErrMsg));
    if (sErrMsg == NULL)
        strncpy(m_sErrMsg, mysql_error(&m_MYSQL), sizeof(m_sErrMsg));
    else
        strncpy(m_sErrMsg, sErrMsg, sizeof(m_sErrMsg));
}

int CMySQL::UseDB(char *sDBName)
{
    if ( mysql_select_db(&m_MYSQL, sDBName) != 0)
    {
        SetErrMsg();
        return -1;
    }
    strncpy (m_sDBName, sDBName, sizeof(m_sDBName));
    strncpy (m_MySQLInfo.sDB, sDBName, sizeof(m_MySQLInfo.sDB));
    return 0;
}

void CMySQL::GetErrMsg(char *sErrMsg, int nLen)
{
    strncpy (sErrMsg, m_sErrMsg, nLen);
}

char *CMySQL::GetErrMsg()
{
    return m_sErrMsg;
}

//除查询外
int CMySQL::Query(char *sSQL, int nSQLLen)
{
    /*
    char *pNewSQL;
    int liLen = 0;
    //< malloc new sql .
    pNewSQL = (char *)malloc (sizeof(char)*(2*nSQLLen+1));
    if (pNewSQL == NULL)
    {
        SetErrMsg("malloc new SQL buffer Fail.");
        return -1;
    }else
        memset (pNewSQL, 0, 2*nSQLLen+1);

    liLen = mysql_escape_string (pNewSQL, sSQL, nSQLLen);
    */
    int liRetCode = 0;
    if (IsValid() != 0)
    {
        SetErrMsg("mysql_ping fail.");
        liRetCode = Connect (m_MySQLInfo.sHost, m_MySQLInfo.sUser, m_MySQLInfo.sPasswd, m_MySQLInfo.sDB, m_MySQLInfo.nPort);
        if (liRetCode != 0)
        {
            //if (pNewSQL != NULL) free (pNewSQL);
            return -3;
        }
    }

    if ( mysql_real_query(&m_MYSQL, sSQL, nSQLLen) != 0)
    {
        SetErrMsg();
        //if (pNewSQL != NULL) free (pNewSQL);
        return -1;
    }

    if (m_nAutoCommit == 0)
    {
        if ( mysql_real_query(&m_MYSQL, "commit", 6) !=0)
        {
            SetErrMsg();
            //if (pNewSQL != NULL) free (pNewSQL);
            return -2;
        }
    }
    //if (pNewSQL != NULL) free (pNewSQL);
    return 0;
}

//影响的行数
unsigned long long CMySQL::AffectedRows()
{
    return mysql_affected_rows(&m_MYSQL);
}

//< return Rows Num.
int CMySQL::GetRecordNum ()
{
    int liRetCode = 0;
    MYSQL_RES *pRes;

    if ( (pRes = mysql_store_result(&m_MYSQL)) == NULL)
    {
        SetErrMsg();
        return -1;
    }

    liRetCode = mysql_num_rows(pRes);

    mysql_free_result(pRes);
    return liRetCode;
}

void CMySQL::SetAutoCommit(int nAutoCommit)
{
    if (nAutoCommit>1 || nAutoCommit <0)
        return ;
    m_nAutoCommit = nAutoCommit;
}

MYSQL_ROW CMySQL::FetchLocateRecord (int *nCount, int nLocate)
{
    MYSQL_RES *pRes;
    MYSQL_ROW  lRow;
    char lsTemp[200] = {0};

    if ( (pRes = mysql_store_result(&m_MYSQL)) == NULL)
    {
        SetErrMsg();
        return NULL;
    }
    //< Get Record Number.
    *nCount = mysql_num_rows(pRes);
    if (nLocate >*nCount-1 || nLocate < 0)
    {
        snprintf (lsTemp, sizeof(lsTemp), "Locate Num Error:%d, (0 ~ %d)", nLocate, *nCount);
        SetErrMsg (lsTemp);
        return NULL;
    }

    //< locate record.
    mysql_data_seek (pRes, nLocate);

    //< fetch record.
    lRow = mysql_fetch_row (pRes);
    mysql_free_result(pRes);

    return lRow;
}


int CMySQL::FetchBatchRecord (int *nRealCount, int nCount, ProcessBatchRecord *pBatchProcessFun)
{
    MYSQL_RES *pRes;
    MYSQL_ROW  lRow;
    int liRetCode = 0, liCount=0;

    if ( (pRes = mysql_store_result(&m_MYSQL)) == NULL)
    {
        SetErrMsg();
        return -1;
    }
    //< Get Record Number.
    *nRealCount = mysql_num_rows(pRes);

    liCount = nCount<=*nRealCount? nCount:*nRealCount;

    for (int i=0; i< liCount; i++)
    {
        mysql_data_seek(pRes, i);
        lRow = mysql_fetch_row (pRes);
        liRetCode = pBatchProcessFun (lRow);
        //if (liRetCode == 0)
    }

    mysql_free_result(pRes);
    return liRetCode;
}

int CMySQL::IsValid ()
{
    int liRetCode = 0;
    liRetCode = mysql_ping (&m_MYSQL);

    return liRetCode;
}

//查询使用
int CMySQL::Query (char *sSQL, int nSQLLen, int *nCount)
{
    int liRetCode = 0;
    if (IsValid() != 0)
    {
        SetErrMsg("mysql_ping fail.");
        liRetCode = Connect (m_MySQLInfo.sHost, m_MySQLInfo.sUser, m_MySQLInfo.sPasswd, m_MySQLInfo.sDB, m_MySQLInfo.nPort);
        if (liRetCode != 0)
            return -3;
    }

    if ( mysql_real_query(&m_MYSQL, sSQL, nSQLLen) != 0)
    {
        SetErrMsg();
        return -1;
    }

    if (m_nAutoCommit == 0)
    {
        if ( mysql_real_query(&m_MYSQL, "commit", 6) !=0)
        {
            SetErrMsg();
            return -2;
        }
    }

    //< store result
    if ( (m_pRes = mysql_store_result(&m_MYSQL)) == NULL)
    {
        SetErrMsg();
        return -3;
    }
    //< Get Record Number.
    *nCount = mysql_num_rows(m_pRes);

    return 0;
}

MYSQL_ROW CMySQL::FetchRecord ()
{
    MYSQL_ROW lRow;

    if (m_pRes != NULL)
    {
        lRow = mysql_fetch_row (m_pRes);
        return lRow;
    }
    else
    {
        return NULL;
    }
}

unsigned long * CMySQL::FetchLength ()
{
    unsigned long * pLen;

    if (m_pRes != NULL)
    {
        pLen = mysql_fetch_lengths (m_pRes);
        return pLen;
    }
    else
    {
        return NULL;
    }
}


int CMySQL::ReleaseRes()
{
    if (m_pRes != NULL)
    {
        mysql_free_result(m_pRes);
        m_pRes=NULL;
    }
    return 0;
}

int CMySQL::EscapeString (char *sTo, const char *sFrom, int nFromLen)
{
    int liLen = 0;
    liLen = mysql_real_escape_string (&m_MYSQL,sTo, sFrom, nFromLen);

    return liLen;
}

int CMySQL::FieldCount()
{
    return mysql_num_fields(m_pRes);
}



