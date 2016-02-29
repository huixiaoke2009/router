#include <arpa/inet.h>
#include <string.h>
#include <stdarg.h>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <iconv.h>
#include <limits.h>
#include <sstream>
#include "util.h"

using namespace std;
using namespace mmlib;

int CBuffTool::ReadByte(const void* pvBuffer, unsigned char &ucVal)
{
    memcpy(&ucVal, pvBuffer, sizeof(unsigned char));
    return sizeof(unsigned char);
}

int CBuffTool::ReadByte(const void* pvBuffer, char &cVal)
{
    memcpy(&cVal, pvBuffer, sizeof(char));
    return sizeof(char);
}

int CBuffTool::WriteByte(void* pvBuffer, unsigned char ucVal)
{
    memcpy(pvBuffer, &ucVal, sizeof(unsigned char));
    return sizeof(unsigned char);
}

int CBuffTool::WriteByte(void* pvBuffer, char cVal)
{
    memcpy(pvBuffer, &cVal, sizeof(char));
    return sizeof(char);
}


int CBuffTool::ReadShort(const void* pvBuffer, unsigned short &ushVal, int iToHostOrder/* = 1*/)
{
    memcpy(&ushVal, pvBuffer, sizeof(unsigned short));
    if (iToHostOrder == 1)
    {
        ushVal = ntohs(ushVal);
    }
    return sizeof(unsigned short);
}

int CBuffTool::ReadShort(const void* pvBuffer, short &shVal, int iToHostOrder/* = 1*/)
{
    memcpy(&shVal, pvBuffer, sizeof(short));
    if (iToHostOrder == 1)
    {
        shVal = ntohs(shVal);
    }
    return sizeof(short);
}

int CBuffTool::WriteShort(void* pvBuffer, unsigned short ushVal, int iToNetOrder/* = 1*/)
{
    if (iToNetOrder == 1)
    {
        ushVal = htons(ushVal);
    }
    memcpy(pvBuffer, &ushVal, sizeof(unsigned short));
    return sizeof(unsigned short);
}

int CBuffTool::WriteShort(void* pvBuffer, short shVal, int iToNetOrder/* = 1*/)
{
    if (iToNetOrder == 1)
    {
        shVal = htons(shVal);
    }
    memcpy(pvBuffer, &shVal, sizeof(short));
    return sizeof(short);
}


int CBuffTool::ReadInt(const void* pvBuffer, unsigned int &uiVal, int iToHostOrder/* = 1*/)
{
    memcpy(&uiVal, pvBuffer, sizeof(unsigned int));
    if (iToHostOrder == 1)
    {
        uiVal = ntohl(uiVal);
    }
    return sizeof(unsigned int);
}

int CBuffTool::ReadInt(const void* pvBuffer, int &iVal, int iToHostOrder/* = 1*/)
{
    memcpy(&iVal, pvBuffer, sizeof(int));
    if (iToHostOrder == 1)
    {
        iVal = ntohl(iVal);
    }
    return sizeof(int);
}

int CBuffTool::WriteInt(void* pvBuffer, unsigned int uiVal, int iToNetOrder/* = 1*/)
{
    if (iToNetOrder == 1)
    {
        uiVal = htonl(uiVal);
    }
    memcpy(pvBuffer, &uiVal, sizeof(unsigned int));
    return sizeof(unsigned int);
}

int CBuffTool::WriteInt(void* pvBuffer, int iVal, int iToNetOrder/* = 1*/)
{
    if (iToNetOrder == 1)
    {
        iVal = htonl(iVal);
    }
    memcpy(pvBuffer, &iVal, sizeof(int));
    return sizeof(int);
}


int CBuffTool::ReadLong(const void* pvBuffer, unsigned long &ulVal, int iToHostOrder/* = 1*/)
{
    unsigned int uiVal;
    memcpy(&uiVal, pvBuffer, sizeof(unsigned int));
    if (iToHostOrder == 1)
    {
        uiVal = ntohl(uiVal);
    }
    ulVal = uiVal;
    return sizeof(unsigned int);
}

int CBuffTool::ReadLong(const void* pvBuffer, long &lVal, int iToHostOrder/* = 1*/)
{
    int iVal;
    memcpy(&iVal, pvBuffer, sizeof(int));
    if (iToHostOrder == 1)
    {
        iVal = ntohl(iVal);
    }
    lVal = iVal;
    return sizeof(int);
}

int CBuffTool::WriteLong(void* pvBuffer, unsigned long ulVal, int iToNetOrder/* = 1 */)
{
    unsigned int uiVal = static_cast<unsigned int>(ulVal);
    if (iToNetOrder == 1)
    {
        uiVal = htonl(uiVal);
    }
    memcpy(pvBuffer, &uiVal, sizeof(unsigned int));
    return sizeof(unsigned int);
}

int CBuffTool::WriteLong(void* pvBuffer, long lVal, int iToNetOrder/* = 1 */)
{
    int iVal = static_cast<int>(lVal);
    if (iToNetOrder == 1)
    {
        iVal = htonl(iVal);
    }
    memcpy(pvBuffer, &iVal, sizeof(int));
    return sizeof(int);
}


int CBuffTool::ReadLongLong(const void* pvBuffer, unsigned long long &ullVal, int iToHostOrder/* = 0*/)
{
    memcpy(&ullVal, pvBuffer, sizeof(unsigned long long));
    if (iToHostOrder == 1)
    {
        ullVal = ntohll(ullVal);
    }
    return sizeof(unsigned long long);
}

int CBuffTool::ReadLongLong(const void* pvBuffer, long long &llVal, int iToHostOrder/* = 0*/)
{
    memcpy(&llVal, pvBuffer, sizeof(long long));
    if (iToHostOrder == 1)
    {
        llVal = ntohll(llVal);
    }
    return sizeof(long long);
}

#if __WORDSIZE == 64
int CBuffTool::ReadLongLong(const void* pvBuffer, uint64_t &ullVal, int iToHostOrder/* = 0*/)
{
    memcpy(&ullVal, pvBuffer, sizeof(unsigned long long));
    if (iToHostOrder == 1)
    {
        ullVal = ntohll(ullVal);
    }
    return sizeof(unsigned long long);
}

int CBuffTool::WriteLongLong(void* pvBuffer, uint64_t ullVal, int iToNetOrder/* = 0*/)
{
    if (iToNetOrder == 1)
    {
        ullVal = htonll(ullVal);
    }
    memcpy(pvBuffer, &ullVal, sizeof(unsigned long long));
    return sizeof(unsigned long long);
}
#endif

int CBuffTool::WriteLongLong(void* pvBuffer, unsigned long long ullVal, int iToNetOrder/* = 0*/)
{
    if (iToNetOrder == 1)
    {
        ullVal = htonll(ullVal);
    }
    memcpy(pvBuffer, &ullVal, sizeof(unsigned long long));
    return sizeof(unsigned long long);
}

int CBuffTool::WriteLongLong(void* pvBuffer, long long llVal, int iToNetOrder/* = 0*/)
{
    if (iToNetOrder == 1)
    {
        llVal = htonll(llVal);
    }
    memcpy(pvBuffer, &llVal, sizeof(long long));
    return sizeof(long long);
}


int CBuffTool::ReadString(const void* pvBuffer, char *pszVal, int iStrLen)
{
    memcpy(pszVal, pvBuffer, iStrLen);
    return iStrLen;
}

int CBuffTool::WriteString(void* pvBuffer, const char *pszVal, int iStrLen)
{
    memcpy(pvBuffer, pszVal, iStrLen);
    return iStrLen;
}

int CBuffTool::ReadBuf(const void* pSrc, void *pDest, int iLen)
{
    memcpy(pDest, pSrc, iLen);
    return iLen;
}

int CBuffTool::WriteBuf(void* pDest, const void *pSrc, int iLen)
{
    memcpy(pDest, pSrc, iLen);
    return iLen;
}

string& CStrTool::MyReplaceAll(string &strInput, const string &strOld, const string &strNew)
{
    string::size_type pos(0);
    while((pos = strInput.find(strOld)) != string::npos)
    {
        strInput.replace(pos, strOld.length(), strNew);
    }
    return strInput;
}

void CStrTool::MySplit(const string &src, const string &separator, vector<string> &dest)
{
    if(src.size() == 0 || separator.size() == 0)
    {
        return;
    }
    
    string str(src);
    string substring;
    string::size_type start = 0, index;
    while((index =str.find_first_of(separator, start)) != string::npos)
    {
        substring = str.substr(start,index - start);
        dest.push_back(substring);
        start = str.find_first_not_of(separator, index);
        if (string::npos == start)
        {
            return;
        }
    }
    substring = str.substr(start);
    dest.push_back(substring);
}

//MySplit在处理如"123||456"这样的字符串时有错误，MySplit2则不会
void CStrTool::MySplit2(const string &src, const string &separator, vector<string> &dest)
{
    if(src.size() == 0 || separator.size() == 0)
    {
        return;
    }
    
    string str(src);
    string substring;
    string::size_type start = 0, index;
    while((index = str.find_first_of(separator, 0)) != string::npos)
    {
        substring = str.substr(start, index - start);
        dest.push_back(substring);
        str = str.substr(index + 1);
    }
    dest.push_back(str);
}

int CStrTool::JsonArray2Map(const string &strJasonArray, vector<string> &vctRst)
{
    if (strJasonArray.empty())
    {
        return 0;
    }
    string strTmp(strJasonArray);
    string strTag = "[";
    string strRep = "";
    MyReplaceAll(strTmp, strTag, strRep);

    strTag = "]";
    strRep = "";
    MyReplaceAll(strTmp, strTag, strRep);

    const string &separator = ",";
    MySplit(strTmp, separator, vctRst);

    return 0;
}

const char *CStrTool::Str2Hex(const void *pvBuff, int iSize)
{
    const int STR2HEX_MAX_BUFF_LEN = 10240;
    const int CHAR_NUM_PER_BYTE = 3;
    const int MAX_HEX_BUFF_NUM = 5;
    static int iCurHexBuffIdx = 0;
    static char szStr2HexBuff[MAX_HEX_BUFF_NUM][(STR2HEX_MAX_BUFF_LEN+1) * CHAR_NUM_PER_BYTE];

    iCurHexBuffIdx++;
    iCurHexBuffIdx %= MAX_HEX_BUFF_NUM;

    if (iSize > STR2HEX_MAX_BUFF_LEN)
    {
        iSize = STR2HEX_MAX_BUFF_LEN;
    }

    for (int i=0; i<iSize; i++)
    {
        snprintf(szStr2HexBuff[iCurHexBuffIdx] + (i * CHAR_NUM_PER_BYTE), sizeof(szStr2HexBuff[iCurHexBuffIdx]) - (i * CHAR_NUM_PER_BYTE), "%02X ", ((unsigned char*)pvBuff)[i]);
    }

    if (iSize == 0)
    {
        szStr2HexBuff[iCurHexBuffIdx][0] = 0;
    }

    return szStr2HexBuff[iCurHexBuffIdx];
}

const char *CStrTool::Hex2Str(const char *pBuff, int iSize, int *pSize)
{
    static vector<char> buffer;
    buffer.resize(iSize);

    int idx = 0;
    char one_byte[3];
    memset(one_byte, 0x0, sizeof(one_byte));

    for(int i = 0; i < iSize/2; ++i){
        memcpy(one_byte, &pBuff[i * 2], 2);
        buffer[idx++] = strtol(one_byte, NULL, 16);
    }
    buffer[idx] = 0;

    if(pSize) *pSize = idx;

    return &buffer[0];
}

unsigned short CStrTool::CheckSum(const void *pvBuff, int iSize)
{
    unsigned short ushSum = 0;
    const unsigned char *pszBuff = (unsigned char *)pvBuff;

    for (int i = 0; i < iSize/2; ++i)
    {
        ushSum ^= *(short *)((char *)pszBuff + i * 2);
    }

    return ushSum;
}

void CStrTool::StripString(string& Str, const char* pszRemove, char chReplaceWith)
{
    const char * pszStart = Str.c_str();
    const char * p = pszStart;
    for (p = strpbrk(p, pszRemove); p != NULL; p = strpbrk(p + 1, pszRemove))
    {
        Str[p - pszStart] = chReplaceWith;
    }
}

void CStrTool::StripString(char* psStr, const char* pszRemove, char chReplaceWith)
{
    const char * pszStart = psStr;
    const char * p = pszStart;
    for (p = strpbrk(p, pszRemove); p != NULL; p = strpbrk(p + 1, pszRemove))
    {
        *(psStr + (p - pszStart)) = chReplaceWith;
    }
}

void CStrTool::StringReplace(const string& sStr, const string& sOldSub,
    const string& sNewSub, bool bReplaceAll, string& sRes)
{
    if (sOldSub.empty())
    {
        sRes.append(sStr);  // if empty, append the given string.
        return;
    }

    std::string::size_type iStartPos = 0;
    std::string::size_type iPos;
    do
    {
        iPos = sStr.find(sOldSub, iStartPos);
        if (iPos == string::npos)
        {
            break;
        }
        sRes.append(sStr, iStartPos, iPos - iStartPos);
        sRes.append(sNewSub);
        iStartPos = iPos + sOldSub.size();
    } while (bReplaceAll);
    sRes.append(sStr, iStartPos, sStr.length() - iStartPos);
}

string CStrTool::StringReplace(const string& sStr, const string& sOldSub,
                     const string& sNewSub, bool bReplaceAll)
{
    string sRet;
    StringReplace(sStr, sOldSub, sNewSub, bReplaceAll, sRet);
    return sRet;
}

void CStrTool::SplitStringUsing(const string& sFull, const char* pszDelim, vector<string>& vsResult)
{
    int iCurSplitTimes = 0;

    if (sFull.empty())
    {
        vsResult.push_back(sFull);
        return;
    }
    
    //add by mama 2015.7.4
    if(pszDelim == NULL || pszDelim[0] == '\0')
    {
        vsResult.push_back(sFull);
        return;
    }

    string::size_type iBeginIndex, iEndIndex;
    iBeginIndex = 0;
    while (iBeginIndex != string::npos)
    {
        iEndIndex = sFull.find_first_of(pszDelim, iBeginIndex);
        if (iEndIndex == string::npos)
        {
            vsResult.push_back(sFull.substr(iBeginIndex));
            return;
        }

        vsResult.push_back(sFull.substr(iBeginIndex, iEndIndex - iBeginIndex));
        iBeginIndex = iEndIndex + 1;
        iCurSplitTimes++;
    }
}

void CStrTool::JoinStrings(const vector<string>& vsItems, const char* pszDelim, string * psResult)
{
    psResult->clear();
    int iDelimLen = strlen(pszDelim);

    typedef vector<string>::const_iterator Iterator;

    // Compute the size first
    int iLength = 0;
    for (Iterator iter = vsItems.begin(); iter != vsItems.end(); ++iter)
    {
        if (iter != vsItems.begin())
        {
            iLength += iDelimLen;
        }
        iLength += iter->size();
    }
    psResult->reserve(iLength);
    
    for (Iterator iter = vsItems.begin(); iter != vsItems.end(); ++iter)
    {
        if (iter != vsItems.begin())
        {
            psResult->append(pszDelim, iDelimLen);
        }
        psResult->append(iter->data(), iter->size());
    }
}

string CStrTool::JoinStrings(const vector<string>& vsItems, const char* pszDelim)
{
    string sResult;
    JoinStrings(vsItems, pszDelim, &sResult);
    return sResult;
}

string CStrTool::SimpleItoa(int i)
{
    static const int ITOA_BUFFER_LEN = 32;
    static const int ITOA_BUFFER_OFFSET = 11;
    char szBuf[ITOA_BUFFER_LEN];

    char* p = szBuf + ITOA_BUFFER_OFFSET;
    *p-- = '\0';
    if (i >= 0)
    {
        do
        {
            *p-- = '0' + i % 10;
            i /= 10;
        } while (i > 0);
        return p + 1;
    }
    else
    {
        if (i > -10)
        {
            i = -i;
            *p-- = '0' + i;
            *p = '-';
            return p;
        }
        else
        {
            // Make sure the value is not MIN_INT
            i = i + 10;
            i = -i;
            *p-- = '0' + i % 10;

            i = i / 10 + 1;
            do
            {
                *p-- = '0' + i % 10;
                i /= 10;
            } while (i > 0);
            *p = '-';
            return p;
        }
    }
}

void CStrTool::InternalAppend(std::string& sDst, const char* pszFmt, va_list stApList)
{
    // Use 4k bytes for the first try, should be enough for most cases
    char szSpace[4096];

    int iSize = sizeof(szSpace);
    char* pBuff = szSpace;
    int iResult = 0;

    do
    {
        iResult = vsnprintf(pBuff, iSize, pszFmt, stApList);
        if ((iResult >= 0) && iResult < iSize)
        {
            // Fit the buffer exactly
            break;
        }

        if (iResult < 0)
        {
            // Double the size of buffer
            iSize *= 2;
        }
        else
        {
            // Need result+1 exactly
            iSize = iResult + 1;
        }

        pBuff = (char*) (pBuff == szSpace ? malloc(iSize) : realloc(pBuff, iSize));

        if (!pBuff)
        {
            // Is it ok to throw an exception here?
            throw std::bad_alloc();
        }
    }
    while (true);

    sDst.append(pBuff, iResult);

    if (pBuff != szSpace)
    {
        free(pBuff);
    }

    return;
}

std::string CStrTool::Format(const char* pszFmt, ...)
{
    va_list stApList;
    va_start(stApList, pszFmt);
    std::string sResult;
    InternalAppend(sResult, pszFmt, stApList);
    va_end(stApList);
    return sResult;
}

void CStrTool::Append(std::string& sDst, const char* pszFmt, ...)
{
    va_list stApList;
    va_start(stApList, pszFmt);
    InternalAppend(sDst, pszFmt, stApList);
    va_end(stApList);
}

const char *CStrTool::TimeString(time_t tTime)
{
    //允许最多20个参数
    static char szTimeStringBuff[20][32];
    static unsigned char cTimerStringIdx;

    cTimerStringIdx++;

    if (cTimerStringIdx >= 20)
    {
        cTimerStringIdx = 0;
    }

    struct tm * ptm = localtime(&tTime);
    strftime(szTimeStringBuff[cTimerStringIdx], sizeof(szTimeStringBuff[cTimerStringIdx]), "%Y-%m-%d %H:%M:%S", ptm);

    return szTimeStringBuff[cTimerStringIdx];
}

const char *CStrTool::TimeString(struct timeval stTime)
{
    //允许最多20个参数
    static char szTimeStringBuff[20][32];
    static unsigned char cTimerStringIdx;

    cTimerStringIdx++;

    if (cTimerStringIdx >= 20)
    {
        cTimerStringIdx = 0;
    }

    struct tm * ptm = localtime(&stTime.tv_sec);
    int year = 1900 + ptm->tm_year; //从1900年开始的年数
    int month = ptm->tm_mon + 1; //从0开始的月数
    int day = ptm->tm_mday; //从1开始的天数
    int hour = ptm->tm_hour; //从0开始的小时数
    int min = ptm->tm_min; //从0开始的分钟数
    int sec = ptm->tm_sec; //从0开始的秒数

    snprintf(szTimeStringBuff[cTimerStringIdx], sizeof(szTimeStringBuff[cTimerStringIdx]), "%04d-%02d-%02d %02d:%02d:%02d.%06ld", year, month, day, hour, min, sec, stTime.tv_usec);

    return szTimeStringBuff[cTimerStringIdx];
}

time_t CStrTool::FromTimeString(const char *pszTimeStr)
{
    struct tm stTime;
    memset(&stTime, 0, sizeof(stTime));
    strptime(pszTimeStr, "%Y-%m-%d %H:%M:%S", &stTime);
    return mktime(&stTime);
}

const char *CStrTool::IPString(unsigned int uiIP)
{
    //允许最多20个参数
    static char szIPStringBuff[20][16];
    static unsigned char cIPStringIdx;

    cIPStringIdx++;

    if (cIPStringIdx >= 20)
    {
        cIPStringIdx = 0;
    }

    snprintf(szIPStringBuff[cIPStringIdx], sizeof(szIPStringBuff[cIPStringIdx]), "%s", inet_ntoa(*(struct in_addr *)&uiIP));
    return szIPStringBuff[cIPStringIdx];
}

const char *CStrTool::IPString(struct in_addr uiIP)
{
    //允许最多20个参数
    static char szIPStringBuff[20][16];
    static unsigned char cIPStringIdx;

    cIPStringIdx++;

    if (cIPStringIdx >= 20)
    {
        cIPStringIdx = 0;
    }

    snprintf(szIPStringBuff[cIPStringIdx], sizeof(szIPStringBuff[cIPStringIdx]), "%s", inet_ntoa(uiIP));
    return szIPStringBuff[cIPStringIdx];
}

/**
 * @brief 校验名字中是否含有非法字符
 * @param pszInStr 输入字符串，必须以'\0'结尾
 * @param pszOutStr 输出字符串，外部开辟空间，空间不能小于pszInStr
 * @return true-名字合法 false-名字不合法，变更以后的名字会写入pszOutStr中
 *
 * @note 如果名字中出现非法字符，将按照如下方式替换：
 * 1）出现非法单字节字符，将字符替换为"_"
 * 2）如果出现非法双字节字符，将字符替换为"□"
 * 3）如果尾部或中间出现半个中文字符，将该字符替换为"_"
 * 4）允许检查的最大字符串长度1024字节
 *
 */
bool CStrTool::CheckValidNameGBK(const char *pszInStr, char *pszOutStr)
{
    //所有的名字、昵称只允许如下字符：
    // 1）ASCII码表中的数字（0-9）字母（A-Z a-z）和下划线（_）
    // 2）GBK码表中的字符（0xA1A1　/0xA1B2〔/0xA1B3〕/0xA3A8（/0xA3A9）除外）

    if ((pszInStr == NULL)||(pszOutStr == NULL))
    {
        return false;
    }

    const unsigned char *pcNowChar = (const unsigned char *)pszInStr;
    const int MAX_CHECK_NAME_LEN = 1024;

    bool bRetVal = true;

    //add by mama: not allow digit
    /*
    if(*pcNowChar < 0x80 && pszInStr[0] >= '0' && pszInStr[0] <= '9')
    {
        bRetVal = false;
        return bRetVal;
    }
    */
    
    int iStrLen = 0;
    while(*pcNowChar != '\0')
    {
        if(*pcNowChar < 0x80)
        {
            if (((*pcNowChar >= '0')&&(*pcNowChar <= '9'))
                ||((*pcNowChar >= 'A')&&(*pcNowChar <= 'Z'))
                ||((*pcNowChar >= 'a')&&(*pcNowChar <= 'z'))
                ||(*pcNowChar == '_'))
            {
                //有效字符
                pszOutStr[iStrLen] = *pcNowChar;
            }
            else
            {
                //无效单字节字符
                pszOutStr[iStrLen] = '_';
                bRetVal = false;
            }
            pcNowChar++;
            iStrLen++;
        }
        else
        {
            const unsigned char *pcNextChar = pcNowChar+1;
            //printf("[%02X%02X]", *pcNowChar, *pcNextChar);
            if (((*pcNowChar >= 0xA1)&&(*pcNowChar <= 0xA9)&&(*pcNextChar >= 0xA1)&&(*pcNextChar <= 0xFE))      //GBK 1区
                ||((*pcNowChar >= 0xB0)&&(*pcNowChar <= 0xF7)&&(*pcNextChar >= 0xA1)&&(*pcNextChar <= 0xFE))    //GBK 2区
                ||((*pcNowChar >= 0x81)&&(*pcNowChar <= 0xA0)&&(*pcNextChar >= 0x40)&&(*pcNextChar <= 0xFE))    //GBK 3区
                ||((*pcNowChar >= 0xAA)&&(*pcNowChar <= 0xFE)&&(*pcNextChar >= 0x40)&&(*pcNextChar <= 0xA0))    //GBK 4区
                ||((*pcNowChar >= 0xA8)&&(*pcNowChar <= 0xA9)&&(*pcNextChar >= 0x40)&&(*pcNextChar <= 0xA0)))   //GBK 5区
            {
                //GBK字符
                if (((*pcNowChar == 0xA1)&&(*pcNextChar == 0xA1))   //
                    ||((*pcNowChar == 0xA1)&&(*pcNextChar == 0xB2)) //〔
                    ||((*pcNowChar == 0xA1)&&(*pcNextChar == 0xB3)) //〕
                    ||((*pcNowChar == 0xA3)&&(*pcNextChar == 0xA8)) //（
                    ||((*pcNowChar == 0xA3)&&(*pcNextChar == 0xA9)) //）
                    ||((*pcNowChar == 0xA9)&&(*pcNextChar == 0x76)) //﹙
                    ||((*pcNowChar == 0xA9)&&(*pcNextChar == 0x77)) //﹚
                    ||((*pcNowChar == 0xA9)&&(*pcNextChar == 0x7A)) //﹝
                    ||((*pcNowChar == 0xA9)&&(*pcNextChar == 0x7B)) //﹞
                    ||((*pcNowChar == 0xA8)&&(*pcNextChar >= 0xA1)&&(*pcNextChar <= 0xC0))  //带音调的拼音，由于一般的字库只绘制一个字节宽度，但是占用两个字节的存储空间，所以会导致显示问题
                    ||((*pcNowChar >= 0x81)&&(*pcNowChar <= 0xA0)&&(*pcNextChar == 0x7F))   //GBK 3区 7F列
                    ||((*pcNowChar >= 0xAA)&&(*pcNowChar <= 0xFE)&&(*pcNextChar == 0x7F))   //GBK 4区 7F列
                    ||((*pcNowChar >= 0xA8)&&(*pcNowChar <= 0xA9)&&(*pcNextChar == 0x7F)))  //GBK 5区 7F列
                    //||((*pcNowChar >= 0xA8)&&(*pcNowChar <= 0xA9)&&(*pcNextChar >= 0x40)&&(*pcNextChar <= 0xA0)))   //用户自定义区
                {
                    pszOutStr[iStrLen] = 0xA1;
                    pszOutStr[iStrLen+1] = 0xF5;    //非法中文替换为□
                    bRetVal = false;
                }
                else
                {
                    pszOutStr[iStrLen] = *pcNowChar;
                    pszOutStr[iStrLen+1] = *pcNextChar;
                }

                pcNowChar+=2;
                iStrLen+=2;
            }
            else
            {
                //不合法单字节
                pszOutStr[iStrLen] = '_';
                bRetVal = false;
                pcNowChar++;
                iStrLen++;
            }
        }

        if (iStrLen >= MAX_CHECK_NAME_LEN)
        {
            bRetVal = false;
            break;
        }
    }

    pszOutStr[iStrLen] = '\0';

    return bRetVal;
}

int CStrTool::CodeConvert(const char* from_charset, const char* to_charset, const char* inbuf, size_t inlen, char* outbuf, size_t& outbyteslef)
{
    char** pin = const_cast<char**>(&inbuf);
    char** pout = &outbuf;
    iconv_t cd = iconv_open(to_charset, from_charset);
    if (cd == 0)
        return -1;
    memset(outbuf, 0, outbyteslef);
    int ret = 0;
    while (true)
    {
        //printf("before, ret=%d, pin=%x, in=%s, inlen=%d, pout=%x, outlen=%d\n", ret, *pin, Str2Hex(*pin, inlen), inlen, *pout, outbyteslef);
        ret = iconv(cd, pin, &inlen, pout, &outbyteslef);
        //printf("after, ret=%d, pin=%x, inlen=%d, pout=%x, outlen=%d, out=%s\n", ret, *pin, inlen, *pout, outbyteslef, outbuf);
        if (ret==0 || inlen == 0 || outbyteslef == 0)
        {
            break;
        }
        else
        {
            (*pin)++;
            inlen--;
            (*pout)[0]=' ';//如果转换失败，使用空格填充
            (*pout)++;
            outbyteslef--;
        }
    }
    iconv_close(cd);
    return 0;

}


int CStrTool::U2G(const char* inbuf, size_t inlen, char* outbuf, size_t& outlen)
{
    return CodeConvert("utf-8", "gbk", inbuf, inlen, outbuf, outlen);
}

int CStrTool::G2U(const char* inbuf, size_t inlen, char* outbuf, size_t& outlen)
{
    return CodeConvert("gbk", "utf-8", inbuf, inlen, outbuf, outlen);
}

const char *CStrTool::G2U(const char* inbuf, size_t inlen)
{
    //允许最多8个参数，每个值允许最多4096字节，如果超过，请使用其他函数
    static char szIPStringBuff[8][4096];
    static unsigned char cRetIdx;

    cRetIdx++;

    if (cRetIdx >= 8)
    {
        cRetIdx = 0;
    }

    memset(&szIPStringBuff[cRetIdx], 0, sizeof(szIPStringBuff[cRetIdx]));
    size_t outlen = sizeof(szIPStringBuff[cRetIdx]);
    CodeConvert("gbk", "utf-8", inbuf, inlen, (char *)&(szIPStringBuff[cRetIdx]), outlen);
    return szIPStringBuff[cRetIdx];
}

const char *CStrTool::G2U(const char* inbuf)
{
    return G2U(inbuf, strlen(inbuf));
}

int CStrTool::IsUTF8(const char *pszBuf, size_t uBuflen)
{
    static char szBuf[10240] = {0};
    char *pIn = const_cast<char *>(pszBuf);
    char *pOut = szBuf;
    size_t uOutBufLen = sizeof(szBuf);

    iconv_t cd = iconv_open("utf-8", "utf-8");
    if (cd ==  (iconv_t)(-1))
    {
        return 0;
    }

    int nRet = 0;

    nRet = iconv(cd, &pIn, &uBuflen, &pOut, &uOutBufLen);
    if (nRet == -1)
    {
        iconv_close(cd);
        return 0;
    }

    iconv_close(cd);
    return 1;
}

/*
UTF-8 valid format list:
0xxxxxxx
110xxxxx 10xxxxxx
1110xxxx 10xxxxxx 10xxxxxx
11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx


json中的特殊字符，这些需要过滤或者转换
 \" ， \\， \/， \b， \f， \n， \r， \t， \u

*/
string CStrTool::FilterUtf8ForJson(const string &strIn)
{
    int nStrLen     = strIn.length();
    int i            = 0;
    int nBytes      = 0;
    string strOut     = "";
    unsigned char c    = 0;

    while (i < nStrLen)
    {
        c = strIn[i];

        if (c < 0x80)
        {
            // 如果是控制字符，转换成空格
            // 如果是引号或斜线，则增加转义符
            switch (c)
            {
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                strOut += ' ';
                break;
            case '\"':
            case '\\':
                strOut += '\\';
                //break;    // 这里不用break，需要继续后面的加上这个字符
            default:
                strOut += c;
                break;
            }

            ++i;
            continue;
        }

        if ((c & 0xE0) == 0xC0)  //110xxxxx
        {
            nBytes = 1;
        }
        else if ((c & 0xF0) == 0xE0) //1110xxxx
        {
            nBytes = 2;
        }
        else if ((c & 0xF8) == 0xF0) //11110xxx
        {
            nBytes = 3;
        }
        else if ((c & 0xFC) == 0xF8) //111110xx
        {
            nBytes = 4;
        }
        else if ((c & 0xFE) == 0xFC) //1111110x
        {
            nBytes = 5;
        }
        else
        {
            // 非法utf8字符过滤掉
            ++i;
            continue;
        }

        // 判断长度是否足够，如果不够则是非法utf8字符过滤掉
        if (nBytes + i + 1 > nStrLen)
        {
            ++i;
            continue;
        }

        // 判断nBytes字符是否符合规范，不符合则过滤掉
        bool bValid = true;
        for (int j = 1; j <= nBytes; ++j)
        {
            c = strIn[i+j];
            if ((c & 0xC0) != 0x80)
            {
                bValid = false;
                break;
            }
        }

        if (!bValid)
        {
            ++i;
            continue;
        }

        strOut.append(strIn, i, 1+nBytes);
        i += (nBytes+1);
    }


    return strOut;
}

unsigned long long CStrTool::SizeStrToNum(const char *pszSize)
{
    unsigned long long RetSize = strtoull(pszSize, NULL, 10);
    char SizeUnit = pszSize[strlen(pszSize)-1];
    switch(SizeUnit)
    {
        case 'K':
        case 'k':
        {
            RetSize *= 1024;
            break;
        }
        case 'M':
        case 'm':
        {
            RetSize = RetSize * 1024 * 1024;
            break;
        }
        case 'G':
        case 'g':
        {
            RetSize = RetSize * 1024 * 1024 * 1024;
            break;
        }
    }

    return RetSize;
}

/* MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 * 2. sizeof(int) == 4
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */
unsigned int CStrTool::MurmurHash2(const void *key, int len, int seed /* = 5381 */)
{
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
 
    /* Initialize the hash to a 'random' value */
    uint32_t h = seed ^ len;
 
    /* Mix 4 bytes at a time into the hash */
    const unsigned char *data = (const unsigned char *)key;
 
    while(len >= 4) {
        uint32_t k = *(uint32_t*)data;
 
        k *= m;
        k ^= k >> r;
        k *= m;
 
        h *= m;
        h ^= k;
 
        data += 4;
        len -= 4;
    }
 
    /* Handle the last few bytes of the input array  */
    switch(len) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0]; h *= m;
    };
 
    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
 
    return (unsigned int)h;
}

//DJB hash function for strings
unsigned int CStrTool::DJBHash(char *str)    
{    
    unsigned int hash = 5381;    
     
    while (*str){    
        hash = ((hash << 5) + hash) + (*str++); /* times 33 */    
    }    
    hash &= ~(1 << 31); /* strip the highest bit */    
    return hash;    
}

int CStrTool::Hash(const char* m_szModuleName)
{
    int h = 0;
    const char *key = m_szModuleName;
    while(*key)
    {
        h = (h << 4) + *key++;
        int g = h & 0xF0000000;
        if(g)
            h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

long long CStrTool::HashStr2LongLong(int UserFrom, const char* pszOpenID)
{
    unsigned int OpenID = 0;
    //OpenID = Hash(pszOpenID);
    int len = strlen(pszOpenID);
    OpenID = MurmurHash2(pszOpenID, len);
    long long Result = UserFrom;
    Result = (Result<<32) | (OpenID);
    return Result;
}


int CSysTool::LockProc(const char* pszModuleName)
{
    int iRetVal = 0;

    if ((NULL == pszModuleName)||(pszModuleName[0] == '\0'))
    {
        return E_SYS_TOOL_MODULE_NAME;
    }

    /* mod old 避免第一次启动时，mkdir /tmp/pid
    char PID_PATH[20] = "/tmp/pid";
    */
    char PID_PATH[20] = "/tmp/";
    /* Vincent delete 可能会导致下面的open文件失败
    if (getenv("HOME") != NULL)
    {
        snprintf(PID_PATH, sizeof(PID_PATH), "%s/pid", getenv("HOME"));
    }
    */

    int iPidFD;
    char szPidFile[1024] = {0};
    snprintf(szPidFile, sizeof(szPidFile), "%s/%s.pid", PID_PATH, pszModuleName);

    if ((iPidFD = open(szPidFile, O_RDWR | O_CREAT, 0644)) == -1)
    {
        return E_SYS_TOOL_OPEN_FILE;
    }

    struct flock stLock;
    stLock.l_type = F_WRLCK;
    stLock.l_whence = SEEK_SET;
    stLock.l_start = 0;
    stLock.l_len = 0;

    iRetVal = fcntl(iPidFD, F_SETLK, &stLock);
    if (iRetVal == -1)
    {
        return E_SYS_TOOL_LOCK_FILE;
    }

    char szLine[16] = {0};
    snprintf(szLine, sizeof(szLine), "%d\n", getpid());
    ftruncate(iPidFD, 0);
    write(iPidFD, szLine, strlen(szLine));

    return SUCCESS;
}

int CSysTool::DaemonInit()
{
    pid_t pid = fork();

    if (pid == -1)
    {
        return E_SYS_TOOL_FORK;
    }
    else if (pid != 0)
    {
        // Parent exits.
        exit(0);
    }

    // 1st child continues.
    setsid(); // Become session leader.
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid != 0)
    {
        exit(0); // First child terminates.

    }

    // clear our file mode creation mask.
    umask(0);

    return SUCCESS;
}

const char *CSysTool::GetNicAddr(const char *pszIfName)
{
    const int MAX_NIC_NUM = 32;

    static char szNicAddr[20] = { 0 };

    if (pszIfName == NULL)
    {
        return NULL;
    }

    memset(szNicAddr, 0x0, sizeof(szNicAddr));

    int iSocket, iIfNum;

    struct ifreq stIfReqBuff[MAX_NIC_NUM];
    struct ifconf stIfConf;

    if ((iSocket = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
    {
        stIfConf.ifc_len = sizeof stIfReqBuff;
        stIfConf.ifc_buf = (caddr_t) stIfReqBuff;
        if (!ioctl(iSocket, SIOCGIFCONF, (char *) &stIfConf))
        {
            iIfNum = stIfConf.ifc_len / sizeof(struct ifreq);

            if ((iIfNum > 0)&&(iIfNum < MAX_NIC_NUM))
            {
                //网卡数量正确
                for (int i=0; i<iIfNum; i++)
                {
                    if (strcmp(stIfReqBuff[i].ifr_name, pszIfName) == 0)
                    {
                        //网卡名称正确
                        if (!(ioctl(iSocket, SIOCGIFADDR, (char *) &stIfReqBuff[i])))
                        {
                            snprintf(szNicAddr, sizeof(szNicAddr), "%s", inet_ntoa(((struct sockaddr_in *) (&stIfReqBuff[i].ifr_addr))->sin_addr));

                            //已经获取到IP地址
                            close(iSocket);
                            return szNicAddr;
                        }
                    }
                }
            }
        }
    }

    close(iSocket);
    return NULL;
}

long long CSysTool::GetFileSize(const char * pszFileName)
{
    int iRetVal = 0;
    struct stat stFileStat;

    int iFD = open(pszFileName, O_RDONLY);
    if (iFD < 0)
    {
        return -1;
    }
    else
    {
        iRetVal = fstat(iFD, &stFileStat);
        if (iRetVal != 0)
        {
            close(iFD);
            return -1;
        }
        else
        {
            close(iFD);
            return stFileStat.st_size;
        }
    }
}

long long CSysTool::GetFileSize(int iFD)
{
    int iRetVal = 0;
    struct stat stFileStat;

    if (iFD < 0)
    {
        return -1;
    }
    else
    {
        iRetVal = fstat(iFD, &stFileStat);
        if (iRetVal != 0)
        {
            return -1;
        }
        else
        {
            return stFileStat.st_size;
        }
    }
}

time_t CSysTool::GetFileMTime(const char * pszFileName)
{
    int iRetVal = 0;
    struct stat stFileStat;

    iRetVal = stat(pszFileName, &stFileStat);
    if (iRetVal != 0)
    {
        return -1;
    }

    return stFileStat.st_mtim.tv_sec;
}

bool CSysTool::IsSameDay(time_t t1, time_t t2)
{
    struct tm st_tm_t1;
    struct tm st_tm_t2;
    localtime_r(&t1, &st_tm_t1);
    localtime_r(&t2, &st_tm_t2);
    return st_tm_t1.tm_yday == st_tm_t2.tm_yday && st_tm_t1.tm_year == st_tm_t2.tm_year;
}

int CSysTool::diff_days(time_t t1, time_t t2)
{
    //首先搞出本地时间和格林威治时间的差距，北京时间比格林威治时间时间早八个小时
    const time_t day_sec = 24*60*60;
    struct tm st_tm_local;
    struct tm st_tm_gm;
    time_t tm_zero = 0;
    localtime_r(&tm_zero, &st_tm_local);
    gmtime_r(&tm_zero, &st_tm_gm);
    time_t diff_local_gm = (st_tm_local.tm_hour - st_tm_gm.tm_hour)*60*60+
                           (st_tm_local.tm_min  - st_tm_gm.tm_min )*60+
                           (st_tm_local.tm_sec  - st_tm_gm.tm_sec );

    time_t start(0), end(0);
    if(t1 > t2)
    {
        start = t2;
        end = t1;
    }
    else
    {
        start = t1;
        end = t2;
    }
    return ((end+diff_local_gm)/day_sec - (start+diff_local_gm)/day_sec);
}

bool CSysTool::IsSameWeek(time_t t1, time_t t2)
{
    int dfdays = diff_days(t1, t2);
    if(dfdays >= 7)
    {
        return false;
    }
    struct tm st_tm_t1;
    struct tm st_tm_t2;
    localtime_r(&t1, &st_tm_t1);
    localtime_r(&t2, &st_tm_t2);
    if(st_tm_t1.tm_wday==0)
        st_tm_t1.tm_wday = 7;
    if(st_tm_t2.tm_wday==0)
        st_tm_t2.tm_wday = 7;
    if(t1 > t2)
    {
        if(st_tm_t1.tm_wday >= st_tm_t2.tm_wday)
            return true;
    }
    else
    {
        if(st_tm_t2.tm_wday >= st_tm_t1.tm_wday)
            return true;
    }
    return false;
}

time_t CSysTool::NextWeek(time_t timestamp)
{
    time_t tOld = timestamp;
    struct tm tmOld;
    localtime_r(&tOld, &tmOld);
    tmOld.tm_sec = 0;
    tmOld.tm_min = 0;
    tmOld.tm_hour = 0;
    // 这里是精确的以周一作为一周开始的下一周判断
    return mktime(&tmOld) + (((7 - tmOld.tm_wday) % 7) + 1) * (60*60*24);
}

int CSysTool::GetDaysInMonth(time_t timestamp)
{
    struct tm st_tm;
    localtime_r(&timestamp, &st_tm);

    int y = st_tm.tm_year + 1900;
    int m = st_tm.tm_mon+1;
    int d = 31;
    int day[]= {31,28,31,30,31,30,31,31,30,31,30,31};
    if (2==m)
    {
        d=((((0==y%4)&&(0!=y%100))||(0==y%400))?29:28);
    }
    else
    {
        d=day[m-1];
    }
    return d;
}

int CRandomTool::Get(int iMin, int iMax)
{
    int iResult = (iMin + (int)((iMax - iMin) * (rand() / (RAND_MAX + 1.0))));
    return iResult;
}
int CRandomTool::GetMultiNum(int iMin,int iMax,unsigned int iNum,std::set<int> &set)
{
    while(set.size() != iNum)
    {
        int iResult = (iMin + (int)((iMax - iMin) * (rand() / (RAND_MAX + 1.0))));
        set.insert(iResult);
    }
    return 0;
}
CRandomTool::CRandomTool()
{
    srand(time(NULL) % getpid());
}
std::vector<SWeightRandomItem>::iterator CRandomTool::WeightRandom
                        ( std::vector<SWeightRandomItem> &vctItem)
{
    int iWeithSum = 0;

    vector<SWeightRandomItem>::iterator itrItem;
    for (itrItem = vctItem.begin(); itrItem != vctItem.end(); itrItem++)
    {
        iWeithSum += itrItem->iWeight;
    }

    //TODO; need to check
    int iRand = CRandomTool::Instance()->Get(0, iWeithSum);// CRand::Random(iWeithSum);


    int iLeft = 0;
    int iRight = 0;

    for (itrItem = vctItem.begin(); itrItem != vctItem.end(); itrItem++)
    {
        iRight += itrItem->iWeight;
        if (iRand >= iLeft && iRand < iRight)
        {
            return itrItem;
        }

        iLeft = iRight;

    }

    return itrItem;
}
int CRandomTool::FrontWeightRandom(std::vector<SWeightRandomItem> &vctRstItem,
                         std::vector<SWeightRandomItem> &vctItem, int iFront /* = 1 */, bool bEnableSameRstItem /*=true*/)
{

    vctRstItem.clear();
    vector<SWeightRandomItem> vctTmpAllItem(vctItem);
    vector<SWeightRandomItem>::iterator itrItem;

    while(!vctTmpAllItem.empty() &&  iFront-- > 0 )
    {
        itrItem = WeightRandom(vctTmpAllItem);
        if (itrItem == vctTmpAllItem.end())
        {
            return -1;
        }
        vctRstItem.push_back(*itrItem);
        if (!bEnableSameRstItem)
        {
            vctTmpAllItem.erase(itrItem);
        }
    }

    return 0;
}
int CTimeCmp::CmpByHour(time_t leftTime,time_t rightTime,int hourNum)
{
    struct tm *pLeft   = localtime(&leftTime);
    struct tm stLeft;
    memcpy(&stLeft, pLeft, sizeof(tm));

    struct tm *pRight   = localtime(&rightTime);
    struct tm stRight;
    memcpy(&stRight, pRight, sizeof(tm));

    return ((stRight.tm_year -stLeft.tm_year)*365*24/hourNum + (stRight.tm_yday -stLeft.tm_yday)*24/hourNum+(stRight.tm_hour -stLeft.tm_hour)/hourNum);

}
int CTimeCmp::CmpByDay(time_t leftTime, time_t rightTime)
{
    struct tm *pLeft   = localtime(&leftTime);
    struct tm stLeft;
    memcpy(&stLeft, pLeft, sizeof(tm));

    struct tm *pRight   = localtime(&rightTime);
    struct tm stRight;
    memcpy(&stRight, pRight, sizeof(tm));


    if (stLeft.tm_year > stRight.tm_year)
    {
        return 1;
    }

    if (stLeft.tm_year < stRight.tm_year)
    {
        return -1;
    }

    if (stLeft.tm_yday == stRight.tm_yday)
    {
        return 0;
    }


    if (stLeft.tm_yday > stRight.tm_yday)
    {
        return 1;
    }

    if (stLeft.tm_yday < stRight.tm_yday)
    {
        return  -1;
    }

    return   0;
}

int CTimeCmp::CmpByWeek(time_t leftTime, time_t rightTime)
{
    int iRet;
    time_t MondayTime;
    struct tm stMondayTime;
    strptime("1970-01-05 00:00:00","%Y-%m-%d %H:%M:%S",&stMondayTime);
    MondayTime = mktime(&stMondayTime);

    unsigned int uiDiffLeft = (leftTime - MondayTime ) / (60 * 60 * 24);
    unsigned int uiDiffRight  = (rightTime - MondayTime) /(60 * 60 * 24);

    iRet = (int )(uiDiffLeft / 7  - uiDiffRight / 7);

    return   iRet;
}

int CTimeCmp::CmpByMonth(time_t leftTime, time_t rightTime)
{
    struct tm *pLeft   = localtime(&leftTime);
    struct tm stLeft;
    memcpy(&stLeft, pLeft, sizeof(tm));

    struct tm *pRight   = localtime(&rightTime);
    struct tm stRight;
    memcpy(&stRight, pRight, sizeof(tm));

    if (stLeft.tm_year > stRight.tm_year)
    {
        return 1;
    }

    if (stLeft.tm_year < stRight.tm_year)
    {
        return -1;
    }

    if (stLeft.tm_mon == stRight.tm_mon)
    {
        return  0;
    }

    if (stLeft.tm_mon > stRight.tm_mon)
    {
        return  1;
    }

    if (stLeft.tm_mon < stRight.tm_mon)
    {
        return  -1;
    }

    return   0;
}


string CHtmlUtil::simpleHtmlEncode(string src)
{
    ostringstream oss;

    char c;
    unsigned int len = src.length();
    for(unsigned int i=0; i < len; ++i)
    {
        c = src[i];
        if (c == '<')
        {
            oss << "&lt;";
        }
        else if (c == '>')
        {
              oss << "&gt;";
        }
        else if (c == '&')
        {
            oss << "&amp;";
        }
        else if (c == '\"')
        {
            oss << "&quot;";
        }
        else if(c == '\'')
        {
            oss << "&#39;";
        }
        else if (c == ' ')
        {
            oss << "&nbsp;";
        }
        else
        {
            oss << c;
        }

    }

    return oss.str();
}

void CHtmlUtil::cutGBRaw( char *szInfo, unsigned int uiMaxLen )
{
    szInfo[uiMaxLen-1] = 0;

    for( unsigned int i = 0;  szInfo[i]!=0; i++ )
    {
        //if( szInfo[i] < 0 )
        if((unsigned char) szInfo[i] > 126) //这样修改，就能把127这个字符扼杀了
        {
            if( (unsigned char)szInfo[i] < 0xA1 || (unsigned char)szInfo[i] > 0xF7 )
            {
                //不是GB码用空格代替
                szInfo[i] = ' ';
                if(szInfo[++i] == 0)
                {
                    break;
                }
                szInfo[i] = ' ';
            }
            else
            {
                if(szInfo[++i] == 0) //是最后1个字符，半中文去掉
                {
                    szInfo[i-1] = 0;
                    break;
                }
            }
        }
    }

}

string CHtmlUtil::cutGB(string str)
{
    unsigned int uiMaxLen = str.length()+1;
    char* buff = new char[uiMaxLen];
    strncpy(buff, str.c_str(), uiMaxLen);
    cutGBRaw(buff, uiMaxLen);
    string ret = buff;
    delete[] buff;
    return ret;
}
