#ifndef UTIL_H_
#define UTIL_H_

#include <stdarg.h>
#include <string>
#include <vector>
#include <inttypes.h>
#include <sys/time.h>
#include <set>
#include <arpa/inet.h>

namespace mmlib
{

inline void showbyte(char* buf, int len)
{
    for(int i = 0; i < len; i++)
    {
        printf("%.2x", buf[i]);
    }
    printf("\n");
}

//0x1234567801234567
inline long long htonll(long long llHost)
{
    long long llNet = 0;
    long long tmpLow = (long long)ntohl((int)llHost) << 32;     //tmpLow BitMap: 0000000001234567
    long long tmpHigh = ntohl((int)(llHost >> 32));             //tmpHigh BitMap:1234567800000000
    llNet = tmpLow | tmpHigh;                                    //llNet BitMap:  1234567801234567
    return llNet;
}

inline long long ntohll(long long llNet)
{
    long long llHost = 0;
    long long tmpLow = (long long)ntohl((int)llNet) << 32;
    long long tmpHigh = ntohl((int)(llNet >> 32));
    llHost = tmpLow | tmpHigh;
    return llHost;
}

class CBuffTool
{
public:

    static int ReadByte(const void *pvBuffer, unsigned char &ucVal);
    static int ReadByte(const void *pvBuffer, char &cVal);
    static int WriteByte(void *pvBuffer, unsigned char ucVal);
    static int WriteByte(void *pvBuffer, char cVal);

    static int ReadShort(const void *pvBuffer, unsigned short &ushVal, int iToHostOrder = 1);
    static int ReadShort(const void *pvBuffer, short &shVal, int iToHostOrder = 1);
    static int WriteShort(void *pvBuffer, unsigned short ushVal, int iToNetOrder = 1);
    static int WriteShort(void *pvBuffer, short shVal, int iToNetOrder = 1);

    static int ReadInt(const void *pvBuffer, unsigned int &uiVal, int iToHostOrder = 1);
    static int ReadInt(const void *pvBuffer, int &iVal, int iToHostOrder = 1);
    static int WriteInt(void *pvBuffer, unsigned int uiVal, int iToNetOrder = 1);
    static int WriteInt(void *pvBuffer, int iVal, int iToNetOrder = 1);

    static int ReadLong(const void *pvBuffer, unsigned long &ulVal, int iToHostOrder = 1);
    static int ReadLong(const void *pvBuffer, long &lVal, int iToHostOrder = 1);
    static int WriteLong(void *pvBuffer, unsigned long ulVal, int iToNetOrder = 1);
    static int WriteLong(void *pvBuffer, long lVal, int iToNetOrder = 1);

    static int ReadLongLong(const void *pvBuffer, unsigned long long &ullVal, int iToHostOrder = 0);
    static int ReadLongLong(const void *pvBuffer, long long &llVal, int iToHostOrder = 0);
    static int WriteLongLong(void *pvBuffer, unsigned long long ullVal, int iToNetOrder = 0);
    static int WriteLongLong(void *pvBuffer, long long llVal, int iToNetOrder = 0);

#if __WORDSIZE == 64
    static int ReadLongLong(const void *pvBuffer, uint64_t &llVal, int iToHostOrder = 0);
    static int WriteLongLong(void *pvBuffer, uint64_t llVal, int iToNetOrder = 0);
#endif

    static int ReadString(const void *pvBuffer, char *pszVal, int iStrLen);
    static int WriteString(void *pvBuffer, const char *pszVal, int iStrLen);

    static int ReadBuf(const void *pvBuffer, void *pszVal, int iStrLen);
    static int WriteBuf(void *pvBuffer, const void *pszVal, int iStrLen);
};

class CStrTool
{
    static void InternalAppend(std::string& sDst, const char* pszFmt, va_list stApList);
public:

    static std::string& MyReplaceAll(std::string &strInput, const std::string &strOld, const std::string &strNew);
    static void MySplit(const std::string &src, const std::string &separator, std::vector<std::string> &dest);
    //MySplit在处理如"123||456"这样的字符串时有错误，MySplit2则不会
    static void MySplit2(const std::string &src, const std::string &separator, std::vector<std::string> &dest);
    static int JsonArray2Map(const std::string &strJasonArray, std::vector<std::string> &vctRst);

    static const char *Str2Hex(const void *pBuff, int iSize);
    // By michaelzhao 2010-08-23 为什么有Str2Hex就没有Hex2Str
    static const char* Hex2Str(const char *pBuff, int iSize, int *pSize);

    static unsigned short CheckSum(const void *pvBuff, int iSize);

    // Added by Jiffychen@2010-03-30
    // Replace any characters in pszRemove with chReplaceWith
    static void StripString(std::string& Str, const char* pszRemove, char chReplaceWith);
    static void StripString(char* psStr, const char* pszRemove, char chReplaceWith);
    // Replace any occurance of sOldSub in sStr by sNewSub,
    // The result is appended to psRes, if bReplaceAll is false, only the first occurance will be replaced
    static void StringReplace(const std::string& sStr, const std::string& sOldSub,
        const std::string& sNewSub, bool bReplaceAll, std::string& sRes);

    // See above, the result is returned
    static std::string StringReplace(const std::string& sStr, const std::string& sOldSub,
        const std::string& sNewSub, bool bReplaceAll);

    // Split sFull by any character in pszDelim
    static void SplitStringUsing(const std::string& sFull,
        const char* pszDelim, std::vector<std::string>& vsResult);

    // Join the items with pszDelim as a separator
    static void JoinStrings(const std::vector<std::string>& vsItems, const char* pszDelim,
        std::string * psResult);

    static std::string JoinStrings(const std::vector<std::string>& vsItems,
        const char* pszDelim);

    // Convert the given int to its string representation
    static std::string SimpleItoa(int i);

    // Format to string
    static std::string Format(const char* pszFmt, ...);

    // Append mode
    static void Append(std::string& sDst, const char* pszFmt, ...);

    // time_t to str
    static const char *TimeString(time_t tTime);
    static const char *TimeString(struct ::timeval stTime);
    
    // str to time_t
    static time_t FromTimeString(const char *pszTimeStr);

    //ip addr to str
    static const char *IPString(unsigned int uiIP);
    static const char *IPString(struct in_addr uiIP);
    
    
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
    static bool CheckValidNameGBK(const char *pszInStr, char *pszOutStr);
    
    static int CodeConvert(const char* from_charset,const char* to_charset, const char* inbuf, size_t inlen, char* outbuf, size_t& outbyteslef);
    static int U2G(const char* inbuf, size_t inlen, char* outbuf, size_t& outlen);
    static int G2U(const char* inbuf, size_t inlen, char* outbuf, size_t& outlen);
    static const char *G2U(const char* inbuf, size_t inlen);
    static const char *G2U(const char* inbuf);
    static int IsUTF8(const char *pszBuf, size_t uBuflen);

    /* 过滤utf8编码字符串中的非法字符和含有引起解析json失败的字符，比如\" ， \\， \/， \b， \f， \n， \r， \t， \u
     * 请确保strIn是utf8编码，返回符合utf8规范的字符串
     */
    static std::string FilterUtf8ForJson(const std::string &strIn);
    
    static unsigned long long SizeStrToNum(const char *pszSize);
    
    static unsigned int MurmurHash2(const void *key, int len, int seed = 5381);
    static unsigned int DJBHash(char *str);
    static int Hash(const char* m_szModuleName);
    static long long HashStr2LongLong(int UserFrom, const char* pszOpenID);
};

class CSysTool
{
public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;

    const static int E_SYS_TOOL_MODULE_NAME = -101;
    const static int E_SYS_TOOL_OPEN_FILE = -102;
    const static int E_SYS_TOOL_LOCK_FILE = -103;
    const static int E_SYS_TOOL_FORK = -104;

    /*
     * @brief 检测是否存在相同进程
     * @param:pszModuleName 模块名
     *
     * @note 该函数调用会创建pid文件（/tmp/<ModuleName>.pid），并且锁定该文件
     */
    static int LockProc(const char* pszModuleName);

    static int DaemonInit();

    static const char *GetNicAddr(const char *pszIfName);

    static long long GetFileSize(const char * pszFileName);
    static long long GetFileSize(int iFD);

    static time_t GetFileMTime(const char * pszFileName);

    static bool IsSameDay(time_t t1, time_t t2);

    static int diff_days(time_t t1, time_t t2);

    static bool IsSameWeek(time_t t1, time_t t2);

    /* 得到下一个星期一的时间戳 */
    static time_t NextWeek(time_t timestamp);
    
    static int GetDaysInMonth(time_t timestamp);
};

typedef struct tagSWeightRandomItem
{
    int iID;        //待抽取物品ID
    int iWeight;    //ID所对应权重
    int iExt;       //附加信息

    tagSWeightRandomItem()
    {
        iID = 0;
        iWeight = 0;
        iExt = 0;
    }
}SWeightRandomItem;

class CRandomTool
{
public:
    static CRandomTool * Instance()
    {
        static CRandomTool * p = new CRandomTool;

        return p;
    }

    //获取iMin到iMax的随机数，左闭右开[Min,Max)
    static int Get(int iMin, int iMax);


    /*根据权重获取结果
     *@iFront[in]:按权重抽取次数
     *@bEnableSameRstItem[in]:抽取多次时，是否允许抽到同一物品
     *@vctItem[in]:待抽取的所有物品及权重信息
     *@vctRstItem[out]:依据权重抽取结果，结果数跟iFront一致
    */
    static int FrontWeightRandom(std::vector<SWeightRandomItem> &vctRstItem,
                         std::vector<SWeightRandomItem> &vctItem,
                         int iFront = 1, bool bEnableSameRstItem =true);
    /*获得多个在iMin到iMax之间的随机数*/
    static int GetMultiNum(int iMin,int iMax,unsigned int iNum,std::set<int> &set);
protected:
        CRandomTool();
        static std::vector<SWeightRandomItem>::iterator WeightRandom
                            ( std::vector<SWeightRandomItem> &vctItem);

};

class CTimeCmp
{
public:
    static int CmpByDay(time_t leftTime, time_t rightTime);
    static int CmpByWeek(time_t leftTime, time_t rightTime);
    static int CmpByMonth(time_t leftTime, time_t rightTime);
    /*计算在lefttime到righttime之间有几个Hournum值*/
    static int CmpByHour(time_t leftTime, time_t rightTime,int HourNum);
};

/*
 * class CSingleton
 */
template <typename T>
class CSingleton
{
public:
    static T & Instance()
    {
        static T instance;
        return instance;
    }

protected:
    CSingleton() {}
};

class CHtmlUtil
{
    //编码过的字符串可以放到js变量的字符串中，并显示在页面上
public:
    static std::string simpleHtmlEncode(std::string src);

    /**
    GBK编码范围(ch1为首字符，ch2为第二个字符):
    0x81<=ch1<=0xFE && (0x40<=ch2<=0x7E || 0x7E<=ch2<=0xFE)

    GB2312
    0xA1<=ch1<=0xF7 && (0xA1<=ch2<=0xFE)
    */
    static void cutGBRaw( char *szInfo, unsigned int uiMaxLen );

    static std::string cutGB(std::string str);

};

}

#endif

