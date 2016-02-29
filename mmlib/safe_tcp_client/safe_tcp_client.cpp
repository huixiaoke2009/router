#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <fcntl.h>

//LOG只是用作一些特殊的自修复、重试错误等情况，外部接口捕获不到的一些信息输出，还有一些调试信息的输出
//能够通过ErrMsg输出的信息，不会通过LOG输出
#include "log/log.h"
#include "ini_file/ini_file.h"
#include "util/util.h"

/**
 * 共享内存结构说明：
 * 1）单台机器上所有到达其他机器的"地址+端口"全部记录在同一的共享内存中，内存的KEY写死
 * 2）考虑到单台机器连接的其他机器"地址+端口"的情况不会太多，不使用复杂的数据结构来存放连接状态，复杂的数据结构一方面
 *    容易出错，另一方那个面在每次初始化的时候需要校验，对于短连接的情况带来一定的计算量
 * 3）对于长时间没有数据来往的情况（n小时），将节点删除，下次如果再需要收发数据，再进行插入就OK
 * 4）共享数据访问不加锁，为了保证程序的准确性，必须做到以下几点：
 *    a）中间节点删除，只是将最后一个节点拷贝到被删除的节点上，最后一步操作为修改节点数量
 *    b）多个进程同时插入节点，会出现互相覆盖的问题，会导致如下情况：
 *       需要的"IP/PORT"没有插进去，这种情况在下次循环会自动修复
 *       不需要的"IP/PORT"出现了，这种情况会在一定时间以后被删除
 *       对应"IP/PORT"的状态或相关统计数据不正确了，这种情况会在下一次循环根据自身连接情况自动修复
 *    c）请求进入时根据目标"IP/PORT"查到对应的节点，任何修改只是修改该节点相关的状态和统计数据，不能修改IP和端口
 *    d）如果发现不存在的"IP/PORT"，需要将该节点插入，插入按照先增加节点数量，再写节点的原则，可能会导致插入两个相同的节点，第二个节点一段时间以后会被删除
 * 5）宗旨：操作中尽量保证数据的稳定，也要保证不能因为该数据的问题导致数据收发错误，不能因为该共享内存的存在导致程序不稳定，任何的错误出现，API要保证有可靠的自修复能力
 *
 * 具体结构：
 * ----------------------------------------------------------------
 * |             STcpConnStatusHeader 136Byte (IA64)               |
 * ----------------------------------------------------------------
 * |                  STcpConnStatus1 136Byte                     |
 * |                  STcpConnStatus2 136Byte                     |
 * |                  STcpConnStatus3 136Byte                     |
 * |                           ....                               |
 * |                  STcpConnStatusn 136Byte                     |
 * ----------------------------------------------------------------
 *
 *
 * 其他说明：
 * 1）不管有多少个连接，第一次初始化时不会连接对端，只是在收发数据的时候进行连接，而且是用到哪个连接，连接哪个，保证短连接应用的效率。
 *
 */

#include "safe_tcp_client/safe_tcp_client.h"

using namespace mmlib;

const int STCP_MAX_CONN_STATUS_KEEP_TIME = 10 * 3600;   //如果没有请求经过时，连接状态最多保留10小时，重连算作请求

CSafeTcpClient::CSafeTcpClient()
{
    memset(m_astSTcpConnData, 0x0, sizeof(m_astSTcpConnData));
    for(int i=0; i<STCP_MAX_CONN_DATA; i++)
    {
        m_astSTcpConnData[i].iSocket = -1;
    }

    m_iSTcpConnDataNum = 0;
    m_iValidConnNum = 0;
    m_iTimeOut = STCP_DEFAULT_TIMEOUT;
    m_iPeerTimeOut = STCP_DEFAULT_PEERTIMEOUT;
    memset(m_szErrMsg, 0x0, sizeof(m_szErrMsg));

    m_pstCurConnStatus = NULL;
    m_pstCurConnData = NULL;
    m_pStatusMem = NULL;
    m_pstSTcpConnStatus = NULL;

    m_iRetryInterval = STCP_RETRY_TIME_INTER_DEFAULT;
    memset(&m_stFullConnStatus, 0, sizeof(m_stFullConnStatus));
    m_stFullConnStatus.ushStatus = STCP_CONN_STATUS_E_FULL;
}

CSafeTcpClient::~CSafeTcpClient()
{
    Destory();
}

/**
 * @brief 使用配置文件初始化
 */
int CSafeTcpClient::Init(const char *pszConfFile, const char *pszSecName/* = "SAFE_TCP_CLI"*/)
{
    CIniFile objIniFile(pszConfFile);

    if (objIniFile.IsValid())
    {
        int iRetryInterval = 0;
        int iTimeOut = 0;
        int iPeerTimeOut = 0;

        char szSTcpAddrList[1024] = {0};
        int iSTcpPort = 0;

        objIniFile.GetString(pszSecName, "HostList", "", szSTcpAddrList, sizeof(szSTcpAddrList));
        objIniFile.GetInt(pszSecName, "Port", 0, &iSTcpPort);
        objIniFile.GetInt(pszSecName, "RetryInterval", 0, &iRetryInterval);
        objIniFile.GetInt(pszSecName, "TimeOut", 0, &iTimeOut);
        objIniFile.GetInt(pszSecName, "PeerTimeOut", STCP_DEFAULT_PEERTIMEOUT, &iPeerTimeOut);

        if (szSTcpAddrList[0] == '\0')
        {
            std::vector<STcpAddr> vstSTcpAddrList;
            STcpAddr stSTcpAddr;
            int iSTcpAddrNum =0;

            //分字段读取HostX和PortX
            objIniFile.GetInt(pszSecName, "HostNum", 0, &iSTcpAddrNum);
            if (iSTcpAddrNum <= 0 || iSTcpAddrNum > STCP_MAX_CONN_DATA)
            {
                snprintf(m_szErrMsg, sizeof(m_szErrMsg), "init fail: conf %s/HostNum [%d] is not valid", pszSecName, iSTcpAddrNum);
                return -1;
            }

            char szValName[64] = {0};
            char szIPAddr[64] = {0};
            for(int i=0; i<iSTcpAddrNum; i++)
            {
                snprintf(szValName, sizeof(szValName), "Host%d", i+1);
                objIniFile.GetString(pszSecName, szValName, "", szIPAddr, sizeof(szIPAddr));
                stSTcpAddr.uiIPAddr = inet_addr(szIPAddr);

                snprintf(szValName, sizeof(szValName), "Port%d", i+1);
                objIniFile.GetInt(pszSecName, szValName, 0, (int *)&stSTcpAddr.ushPort);

                if (stSTcpAddr.uiIPAddr == 0 || stSTcpAddr.ushPort == 0)
                {
                    snprintf(m_szErrMsg, sizeof(m_szErrMsg), "init fail: conf %s/Host%d [%s] or %s/Port%d [%d] is not valid", pszSecName, i+1, szIPAddr, pszSecName, i+1, stSTcpAddr.ushPort);
                    return -1;
                }

                vstSTcpAddrList.push_back(stSTcpAddr);
            }

            return Init(vstSTcpAddrList, iTimeOut, iRetryInterval, iPeerTimeOut);
        }
        else
        {
            return Init(szSTcpAddrList, iSTcpPort, iTimeOut, iRetryInterval, iPeerTimeOut);
        }
    }
    else
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "init fail: conf file[%s] is not valid", pszSecName);
        return -1;
    }

    return 0;
}

/**
 * @brief 使用多个IP地址，相同的端口初始化
 */
int CSafeTcpClient::Init(const char * pszIPList, int iPort, int iTimeOut/* = STCP_DEFAULT_TIMEOUT*/, int iRetryInterval/* = STCP_RETRY_TIME_INTER_DEFAULT*/, int iPeerTimeOut/* =STCP_DEFAULT_PEERTIMEOUT */)
{
    std::vector<STcpAddr> vstSTcpAddrList;
    STcpAddr stSTcpAddr;

    char szAddrList[1024];
    snprintf(szAddrList, sizeof(szAddrList), "%s", pszIPList);
    char *pszOneIPAddr = NULL;
    int iCurAddrNum = 0;
    for(char *pszSecVal = szAddrList; (pszOneIPAddr = strtok(pszSecVal, ",")) != NULL; pszSecVal=NULL)
    {
        stSTcpAddr.uiIPAddr = inet_addr(pszOneIPAddr);
        stSTcpAddr.ushPort = iPort;
        if (stSTcpAddr.uiIPAddr == 0||stSTcpAddr.ushPort == 0)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "init fail: addr_list[%s] or port [%d] is not valid", szAddrList, stSTcpAddr.ushPort);
            return -1;
        }

        iCurAddrNum++;
        if (iCurAddrNum >= STCP_MAX_CONN_DATA)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "init fail: addr_list[%s] is not valid, addr_num[%d] is too big", szAddrList, iCurAddrNum);
            return -1;
        }

        vstSTcpAddrList.push_back(stSTcpAddr);
    }

    return Init(vstSTcpAddrList, iTimeOut, iRetryInterval, iPeerTimeOut);
}

/**
 * @brief 使用多个IP地址，不同的端口初始化
 */
int CSafeTcpClient::Init(const std::vector<STcpAddr> &vstSTcpAddrList, int iTimeOut/* = STCP_DEFAULT_TIMEOUT*/, int iRetryInterval/* = STCP_RETRY_TIME_INTER_DEFAULT*/, int iPeerTimeOut/* =STCP_DEFAULT_PEERTIMEOUT */)
{
    int iRetVal = 0;

    //防止多次Init，句柄泄漏
    Destory();

    if (vstSTcpAddrList.size() > (unsigned int)STCP_MAX_CONN_DATA)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "init fail: addr list is too big, max[%d], now[%d]", (int)vstSTcpAddrList.size(), STCP_MAX_CONN_DATA);
        return -1;
    }

    m_iSTcpConnDataNum = 0;
    std::vector<STcpAddr>::const_iterator pstSTcpAddr = vstSTcpAddrList.begin();
    for(; pstSTcpAddr != vstSTcpAddrList.end(); pstSTcpAddr++)
    {
        if ((pstSTcpAddr->uiIPAddr == 0) || (pstSTcpAddr->ushPort == 0))
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "init fail: addr list is not valid, ip[%s], port[%d]", inet_ntoa(*(struct in_addr*)(&pstSTcpAddr->uiIPAddr)), pstSTcpAddr->ushPort);
            return -1;
        }

        m_astSTcpConnData[m_iSTcpConnDataNum].uiIPAddr = pstSTcpAddr->uiIPAddr;
        m_astSTcpConnData[m_iSTcpConnDataNum].ushPort = pstSTcpAddr->ushPort;
        m_astSTcpConnData[m_iSTcpConnDataNum].iSocket = -1;

        m_iSTcpConnDataNum++;
    }

    m_iTimeOut = iTimeOut;
    m_iRetryInterval = iRetryInterval;
    m_iPeerTimeOut = iPeerTimeOut;

    if (m_iRetryInterval > (STCP_MAX_CONN_STATUS_KEEP_TIME / 5))
    {
        m_iRetryInterval = (STCP_MAX_CONN_STATUS_KEEP_TIME / 60);    //防止连接状态被清楚
    }

    iRetVal = InitStatusShm();

    if (iRetVal != 0)
    {
        return iRetVal;
    }

    return 0;
}

int CSafeTcpClient::InitStatusShm()
{
    int iRetVal = 0;

    //初始化共享内存
    int iShmExistFlag = 0;  //共享内存存在标志位 0-不存在 1-存在
    iRetVal = m_objStatusMem.Create(STCP_CONN_STATUS_SHM_KEY, STCP_CONN_STATUS_SHM_SIZE, 0666);
    if (iRetVal == m_objStatusMem.SUCCESS)
    {
        //内存不存在，重新创建的
        iShmExistFlag = 0;
    }
    else if (iRetVal == m_objStatusMem.SHM_EXIST)
    {
        //内存存在
        iShmExistFlag = 1;
    }
    else
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "init fail: status_shm create failed, ret=%d", iRetVal);
        return -1;
    }

    iRetVal = m_objStatusMem.Attach();
    if (iRetVal != m_objStatusMem.SUCCESS)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "init fail: status_shm attach failed, ret=%d", iRetVal);
        return -1;
    }

    m_pStatusMem = (char *)m_objStatusMem.GetMem();
    if (m_pStatusMem == (char *)-1)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "init fail: status_shm getmem failed");
        return -1;
    }

    m_pstSTcpConnStatus = (STcpConnStatusHeader *)m_pStatusMem;

    if (iShmExistFlag == 0||m_pstSTcpConnStatus->uiMagicNum != (unsigned int)STCP_STATUS_MEM_MAGICNUM )
    {
        Log(0, 0, LOG_WARN, "%s|%d stcp_status_mem cleared, exist_flag=%d, magic_num=0x%08X", __func__, __LINE__, iShmExistFlag, m_pstSTcpConnStatus->uiMagicNum);
        memset(m_pStatusMem, 0x0, STCP_CONN_STATUS_SHM_SIZE);
        m_pstSTcpConnStatus->ushVersion = STCP_STATUS_MEM_VERSION;
        m_pstSTcpConnStatus->uiMagicNum = STCP_STATUS_MEM_MAGICNUM;
        m_pstSTcpConnStatus->tCreateTime = time(NULL);
    }

    return 0;
}

/**
 * @brief 设置超时时间，单位毫秒
 */
int CSafeTcpClient::SetTimeOut(int iTimeOut)
{
    m_iTimeOut = iTimeOut;
    return 0;
}

STcpConnStatus *CSafeTcpClient::GetSTcpConnStatus(unsigned int uiIPAddr, unsigned short ushPort)
{
    if (m_pstSTcpConnStatus->uiConnStatusNum > (unsigned int)STCP_MAX_CONN_STATUS)
    {
        m_pstSTcpConnStatus->uiConnStatusNum = STCP_MAX_CONN_STATUS;
    }

    for(unsigned int i=0; i<m_pstSTcpConnStatus->uiConnStatusNum; i++)
    {
        if ((m_pstSTcpConnStatus->astSTcpConnStatus[i].uiIPAddr == uiIPAddr) && (m_pstSTcpConnStatus->astSTcpConnStatus[i].ushPort == ushPort))
        {
            return &(m_pstSTcpConnStatus->astSTcpConnStatus[i]);
        }
    }

    return NULL;
}

STcpConnStatus *CSafeTcpClient::AddSTcpConnStatus(unsigned int uiIPAddr, unsigned short ushPort)
{
    //如果已经有了数据，使用原有的数据
    STcpConnStatus *pstRetConnStatus = GetSTcpConnStatus(uiIPAddr, ushPort);
    if (pstRetConnStatus != NULL)
    {
        return pstRetConnStatus;
    }

    if (m_pstSTcpConnStatus->uiConnStatusNum >= (unsigned int)STCP_MAX_CONN_STATUS)
    {
        Log(0, 0, LOG_WARN, "STCP:tcp_conn_data full, ip=%s, port=%d", inet_ntoa(*((struct in_addr*)&uiIPAddr)), ushPort);
        m_pstSTcpConnStatus->uiStatusFullNum++;
        m_stFullConnStatus.uiIPAddr = uiIPAddr;
        m_stFullConnStatus.ushPort = ushPort;
        m_stFullConnStatus.ushStatus = STCP_CONN_STATUS_E_FULL;
        return &m_stFullConnStatus;
    }

    int iConnStatusIDX = m_pstSTcpConnStatus->uiConnStatusNum;
    m_pstSTcpConnStatus->uiConnStatusNum++;

    memset(&m_pstSTcpConnStatus->astSTcpConnStatus[iConnStatusIDX], 0, sizeof(STcpConnStatus));
    m_pstSTcpConnStatus->astSTcpConnStatus[iConnStatusIDX].uiIPAddr = uiIPAddr;
    m_pstSTcpConnStatus->astSTcpConnStatus[iConnStatusIDX].ushPort = ushPort;
    m_pstSTcpConnStatus->astSTcpConnStatus[iConnStatusIDX].uiTimeOut = m_iTimeOut;
    m_pstSTcpConnStatus->astSTcpConnStatus[iConnStatusIDX].uiLastReqNum = 1;    //防止被删除

    return &(m_pstSTcpConnStatus->astSTcpConnStatus[iConnStatusIDX]);
}

int CSafeTcpClient::DelConnStatusInternal(unsigned int uiIPAddr, unsigned short ushPort)
{
    if (m_pstSTcpConnStatus->uiConnStatusNum > (unsigned int)STCP_MAX_CONN_STATUS)
    {
        m_pstSTcpConnStatus->uiConnStatusNum = STCP_MAX_CONN_STATUS;
    }

    for(unsigned int i=0; i<m_pstSTcpConnStatus->uiConnStatusNum; i++)
    {
        if ((m_pstSTcpConnStatus->astSTcpConnStatus[i].uiIPAddr == uiIPAddr) && (m_pstSTcpConnStatus->astSTcpConnStatus[i].ushPort == ushPort))
        {
            memcpy(&m_pstSTcpConnStatus->astSTcpConnStatus[i], &m_pstSTcpConnStatus->astSTcpConnStatus[m_pstSTcpConnStatus->uiConnStatusNum], sizeof(STcpConnStatus));
            m_pstSTcpConnStatus->uiConnStatusNum--;
            break;
        }
    }

    return 0;
}

//对m_pstCurConnData指向的连接进行连接或重连
//iTestConnFlag是用于标识本次连接是否只是探测连接的有效性，如果是探测，重连的超时时间是普通超时时间的1/5
int CSafeTcpClient::ConnectInternal(int iTestConnFlag/* = 0 */)
{
    int iRetVal = 0;
    if (m_pstCurConnData == NULL)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "internal err:%s: conn_data is null", __func__);
        return -1;
    }

    if (m_pstCurConnData->iSocket != -1)
    {
        close(m_pstCurConnData->iSocket);
        m_pstCurConnData->iSocket = -1;
    }

    m_pstCurConnData->iSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (m_pstCurConnData->iSocket == -1)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "internal err: create socket err in ConnectInternal, errno=%d, errmsg=%s", errno, strerror(errno));
        return -1;
    }

    struct sockaddr_in stSockAddr;

    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(m_pstCurConnData->ushPort);
    stSockAddr.sin_addr.s_addr = m_pstCurConnData->uiIPAddr;

    //设置为非阻塞模式
    fcntl( m_pstCurConnData->iSocket, F_SETFL, O_NONBLOCK);

    iRetVal = connect(m_pstCurConnData->iSocket, (struct sockaddr *)&stSockAddr, sizeof(stSockAddr));
    if ((iRetVal == -1) && (errno != EINPROGRESS))
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "connect err: ip=%s, port=%d, errno=%d, errmsg=%s", inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort, errno, strerror(errno));
        return -1;
    }

    int iConnTimeOut = m_iTimeOut;
    if (iTestConnFlag == 1)
    {
        //TODO 这里可以根据实际情况进行调整
        iConnTimeOut = iConnTimeOut/5;
    }
    iRetVal = SendWait(m_pstCurConnData->iSocket, iConnTimeOut);
    if (iRetVal != 0)
    {
        return -1;
    }

    return 0;
}

int CSafeTcpClient::GetConnection()
{
    if (m_pstSTcpConnStatus == NULL)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "internal err:%s: conn_status is null,maybe not init or destoried", __func__);
        return -1;
    }

    int iRetVal = 0;
    int iRandomConnIDX = CRandomTool::Instance()->Get(0, m_iSTcpConnDataNum);
    time_t tTimeNow = time(NULL);

/*
    // debug the connection status
    char szTime[64] = {0};
    Log(0, 0, LOG_DEBUG, "STCP: connection status information");
    snprintf(szTime, sizeof(szTime), "%s", CStrTool::TimeString(tTimeNow));
    Log(0, 0, LOG_DEBUG, "STCP: tTimeNow|%s[%zd]|m_iPeerTimeOut|%d",
                    szTime,
                    tTimeNow,
                    m_iPeerTimeOut);

    for (int i = 0; i < m_iSTcpConnDataNum; i++)
    {
            snprintf(szTime, sizeof(szTime), "%s", CStrTool::TimeString(m_astSTcpConnData[i].tLastUseTime));
            Log(0, 0, LOG_DEBUG, "STCP: <%d> %s:%d|socket|%d|%s[%zd]",
                            i,
                            inet_ntoa(*((struct in_addr*)&m_astSTcpConnData[i].uiIPAddr)),
                            m_astSTcpConnData[i].ushPort,
                            m_astSTcpConnData[i].iSocket,
                            szTime, 
                            m_astSTcpConnData[i].tLastUseTime );
    }
*/

    //TODO 需要对所有连接状态的数据进行检查，频率大概10小时检查一次，将十小时内没有收发数据的连接状态清理掉
    if ((m_pstSTcpConnStatus->tLastCheckTime > (tTimeNow + 600)) || (m_pstSTcpConnStatus->tLastCheckTime < (tTimeNow - STCP_MAX_CONN_STATUS_KEEP_TIME)))
    {
        m_pstSTcpConnStatus->tLastCheckTime = tTimeNow;

        if (m_pstSTcpConnStatus->uiConnStatusNum > (unsigned int)STCP_MAX_CONN_STATUS)
        {
            m_pstSTcpConnStatus->uiConnStatusNum = STCP_MAX_CONN_STATUS;
        }

        for(unsigned int i=0; i<m_pstSTcpConnStatus->uiConnStatusNum; i++)
        {
            if (m_pstSTcpConnStatus->astSTcpConnStatus[i].uiLastReqNum == 0)
            {
                //从上次检查到现在的总请求量为0，需要将该连接状态删除，该连接会在下一次被检测到
                memcpy(&m_pstSTcpConnStatus->astSTcpConnStatus[i], &m_pstSTcpConnStatus->astSTcpConnStatus[m_pstSTcpConnStatus->uiConnStatusNum], sizeof(STcpConnStatus));
                m_pstSTcpConnStatus->uiConnStatusNum--;
            }
            else
            {
                m_pstSTcpConnStatus->astSTcpConnStatus[i].uiLastReqNum = 0;
            }
        }
    }

    int iRetryTimes = 0;
    //TODO 最多允许连续失效m_iSTcpConnDataNum/2 + 1个连接，超过这个数量，不判断重连间隔时间直接重连
    int iMaxRetryTimes = m_iSTcpConnDataNum/2 + 1;
    for (; iRetryTimes < iMaxRetryTimes; iRetryTimes++)
    {
        m_pstCurConnData = &(m_astSTcpConnData[iRandomConnIDX]);
        m_pstCurConnStatus = AddSTcpConnStatus(m_pstCurConnData->uiIPAddr, m_pstCurConnData->ushPort);

        if (m_pstCurConnData->iSocket == -1)
        {
            if (m_pstCurConnStatus->ushStatus == STCP_CONN_STATUS_E_CONN
                ||m_pstCurConnStatus->ushStatus == STCP_CONN_STATUS_E_SEND)
            {
                //对于连接失败，或者发送失败，需要根据状态进行尝试
                if ((tTimeNow - m_pstCurConnStatus->tLastRetryTime) < 0||(tTimeNow - m_pstCurConnStatus->tLastRetryTime) > m_iRetryInterval)
                {
                    iRetVal = ConnectInternal(1);
                    if (iRetVal == 0)
                    {
                        //连接成功的情况，会在后续数据操作完毕以后进行状态更新
                        Log(0, 0, LOG_INFO, "STCP:try reconnect succ, ip=%s, port=%d", inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort)
                        break;
                    }
                    else
                    {
                        //修改状态数据
                        Log(0, 0, LOG_WARN, "STCP:try reconnect fail, errmsg=%s", m_szErrMsg)
                        Log(0, 0, LOG_WARN, "STCP:try reconnect fail, conn_err_num=%d, retry_times=%d, ip=%s, port=%d", m_pstCurConnStatus->uiConnErrNum, m_pstCurConnStatus->uiRetryTimes, inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort)
                        SetSTcpConnStatusInternal(STCP_CONN_STATUS_E_CONN);
                    }
                }
            }
            else
            {
                //首次建立连接，或者不通过Status共享内存控制，超时时间正常
                iRetVal = ConnectInternal();
                if (iRetVal == 0)
                {
                    Log(0, 0, LOG_DEBUG, "STCP:connect succ, ip=%s, port=%d", inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort);
                    break;
                }
                else
                {
                    //返回错误
                    //修改状态数据
                    Log(0, 0, LOG_WARN, "STCP:connect fail, errmsg=%s", m_szErrMsg)
                    Log(0, 0, LOG_WARN, "STCP:connect fail, conn_err_num=%d, retry_times=%d, ip=%s, port=%d", m_pstCurConnStatus->uiConnErrNum, m_pstCurConnStatus->uiRetryTimes, inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort)
                    SetSTcpConnStatusInternal(STCP_CONN_STATUS_E_CONN);
                    return -1;
                }
            }
        }
        else if ((tTimeNow - m_pstCurConnData->tLastUseTime) < 0 || tTimeNow - m_pstCurConnData->tLastUseTime > m_iPeerTimeOut) 
        {
                iRetVal = ConnectInternal(1);
                if (iRetVal == 0)
                {
                        //连接成功的情况，会在后续数据操作完毕以后进行状态更新
                        Log(0, 0, LOG_INFO, "I thought peer tcp connection has closed, so reconnect");
                        Log(0, 0, LOG_INFO, "STCP:try reconnect succ, ip=%s, port=%d", inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort)
                                break;
                }
                else
                {
                        //修改状态数据
                        Log(0, 0, LOG_WARN, "STCP:try reconnect fail, errmsg=%s", m_szErrMsg)
                                Log(0, 0, LOG_WARN, "STCP:try reconnect fail, conn_err_num=%d, retry_times=%d, ip=%s, port=%d", m_pstCurConnStatus->uiConnErrNum, m_pstCurConnStatus->uiRetryTimes, inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort)
                                SetSTcpConnStatusInternal(STCP_CONN_STATUS_E_CONN);
                }
        }
        else
        {
            //只要连接正常，不关心是否状态有问题，继续收发数据
            break;
        }

        //如果发现该连接不可用，也不必重连，则使用下个连接
        iRandomConnIDX = (iRandomConnIDX+1)%m_iSTcpConnDataNum;
    }

    if (iRetryTimes >= iMaxRetryTimes)
    {
        //强制对连续无效的连接中随机一个进行重连
        iRandomConnIDX = CRandomTool::Instance()->Get(0, iMaxRetryTimes);
        int iForceConnIdx = ((m_pstCurConnData - m_astSTcpConnData) + m_iSTcpConnDataNum - iRandomConnIDX) % m_iSTcpConnDataNum;
        m_pstCurConnData = &(m_astSTcpConnData[iForceConnIdx]);
        m_pstCurConnStatus = AddSTcpConnStatus(m_pstCurConnData->uiIPAddr, m_pstCurConnData->ushPort);

        Log(0, 0, LOG_WARN, "STCP:force reconnect svr, ip=%s, port=%d", inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort)

        iRetVal = ConnectInternal();
        if (iRetVal == 0)
        {
            Log(0, 0, LOG_INFO, "STCP:force reconnect succ, ip=%s, port=%d", inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort);
        }
        else
        {
            //返回错误
            //修改状态数据
            Log(0, 0, LOG_WARN, "STCP:force reconnect fail, errmsg=%s", m_szErrMsg)
            Log(0, 0, LOG_WARN, "STCP:force reconnect fail, conn_err_num=%d, retry_times=%d, ip=%s, port=%d", m_pstCurConnStatus->uiConnErrNum, m_pstCurConnStatus->uiRetryTimes, inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort)
            SetSTcpConnStatusInternal(STCP_CONN_STATUS_E_CONN);
            return -1;
        }
    }

    return 0;
}


int CSafeTcpClient::PollWait(int iFd, short shEvent, int iTimeOut)
{
    struct pollfd stPoll;

    stPoll.fd = iFd;
    stPoll.events = shEvent;
    stPoll.revents = 0;

    while(true)
    {
        switch (poll(&stPoll, 1, iTimeOut))
        {
        case -1:
            if (errno != EINTR)
            {
                snprintf(m_szErrMsg, sizeof(m_szErrMsg), "pool fail: errno=%d, errmsg=%s, timeout[%d], host=%s, port=%d", errno, strerror(errno), iTimeOut, inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort);
                return -1;
            }
            continue;

        case 0:
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "poll fail: timeout[%d], host=%s, port=%d", iTimeOut, inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort);
            return -1;

        default:
            if (stPoll.revents & POLLHUP)
            {
                snprintf(m_szErrMsg, sizeof(m_szErrMsg), "poll fail: revents POLLHUP, timeout[%d], host=%s, port=%d", iTimeOut, inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort);
                return -1;
            }
            if (stPoll.revents & POLLERR)
            {
                snprintf(m_szErrMsg, sizeof(m_szErrMsg), "poll fail: revents POLLERR, timeout[%d], host=%s, port=%d", iTimeOut, inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort);
                return -1;
            }
            if (stPoll.revents & POLLNVAL)
            {
                snprintf(m_szErrMsg, sizeof(m_szErrMsg), "poll fail: revents POLLNVAL, timeout[%d], host=%s, port=%d", iTimeOut, inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort);
                return -1;
            }
            if (stPoll.revents & shEvent)
            {
                return 0;
            }
            else
            {
                snprintf(m_szErrMsg, sizeof(m_szErrMsg), "poll fail: revents other error, timeout[%d], host=%s, port=%d", iTimeOut, inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort);
                return -1;
            }
        }
    }
}

int CSafeTcpClient::RecvWait(int iFd, int iTimeOut)
{
    return PollWait(iFd, POLLIN, iTimeOut);
}

int CSafeTcpClient::SendWait(int iFd, int iTimeOut)
{
    return PollWait(iFd, POLLOUT, iTimeOut);
}

/**
 * @brief 将数据发送到m_pstCurConnData指向的socket上
 */
int CSafeTcpClient::SendInternal(const void *pszBuff, unsigned int uiLen)
{
    if ((m_pstCurConnData == NULL) || (m_pstCurConnStatus == NULL))
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "internal err:%s: conn_data or conn_status is null", __func__);
        return -1;
    }

    unsigned int uiBytesSent = 0;
    int iWriteBytes = 0;

    while(true)
    {
        if (SendWait(m_pstCurConnData->iSocket, m_iTimeOut) != 0)
        {
            //修改连接状态数据
            SetSTcpConnStatusInternal(STCP_CONN_STATUS_E_SEND);
            return -1;
        }

        iWriteBytes = write(m_pstCurConnData->iSocket, (unsigned char *)pszBuff + uiBytesSent, uiLen - uiBytesSent);
        if (iWriteBytes > 0)
        {
            uiBytesSent += iWriteBytes;
            if (uiBytesSent < uiLen) continue;
            else break;
        }
        else if (errno == EINTR)
        {
            continue;
        }
        else
        {
            break;
        }
    }

    if (iWriteBytes < 0)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "send err: ip=%s, port=%d, errno=%d, errmsg=%s", inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort, errno, strerror(errno));
        SetSTcpConnStatusInternal(STCP_CONN_STATUS_E_SEND);
        return -1;
    }

    return 0;
}


/**
 * @brief 将数据发送到m_pstCurConnData指向的socket上
 */
int CSafeTcpClient::RecvInternal(void *pszBuff, unsigned int *puiLen, unsigned int uiExpectLen)
{
    if ((m_pstCurConnData == NULL) || (m_pstCurConnStatus == NULL))
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "internal err:%s: conn_data or conn_status is null", __func__);
        return -1;
    }

    unsigned int uiBytesReceived = 0;
    int iReadBytes = 0;

    while (true)
    {
        if (0 != RecvWait(m_pstCurConnData->iSocket, m_iTimeOut))
        {
            //修改连接状态数据
            SetSTcpConnStatusInternal(STCP_CONN_STATUS_E_RECV);
            return -1;
        }

        if (uiExpectLen > 0)
        {
            iReadBytes = read(m_pstCurConnData->iSocket, (unsigned char *)pszBuff + uiBytesReceived, uiExpectLen - uiBytesReceived);
            if (iReadBytes > 0)
            {
                *puiLen = (uiBytesReceived += iReadBytes);
                if (uiBytesReceived < uiExpectLen) continue;
                else break;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                break;
            }
        }
        else
        {
            iReadBytes = read(m_pstCurConnData->iSocket, pszBuff, *puiLen);
            if (iReadBytes > 0)
            {
                *puiLen = iReadBytes;
                return 0;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                break;
            }
        }
    }

    if (iReadBytes <= 0)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "recv err: ip=%s, port=%d, errno=%d, errmsg=%s", inet_ntoa(*((struct in_addr*)&m_pstCurConnData->uiIPAddr)), m_pstCurConnData->ushPort, errno, strerror(errno));
        SetSTcpConnStatusInternal(STCP_CONN_STATUS_E_RECV);
        return -1;
    }

    return 0;
}

void CSafeTcpClient::SetSTcpConnStatusInternal(unsigned short ushConnStatus)
{
    if ((m_pstCurConnStatus == NULL) || (m_pstCurConnData == NULL))
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "internal err:%s: conn_data or conn_status is null", __func__);
        return;
    }

    m_pstCurConnStatus->ushStatus = ushConnStatus;

    switch(ushConnStatus)
    {
        case STCP_CONN_STATUS_OK:
        {
            m_pstCurConnStatus->uiLastReqNum++;
            break;
        }
        case STCP_CONN_STATUS_E_CONN:
        {
            m_pstCurConnStatus->uiRetryTimes++;
            m_pstCurConnStatus->uiConnErrNum++;
            m_pstCurConnStatus->tLastRetryTime = time(NULL);
            m_pstCurConnStatus->uiLastReqNum++;

            if (m_pstCurConnData->iSocket != -1)
            {
                close(m_pstCurConnData->iSocket);
                m_pstCurConnData->iSocket = -1;
            }
            break;
        }
        case STCP_CONN_STATUS_E_RECV:
        {
            m_pstCurConnStatus->uiRecvErrNum++;
            if (m_pstCurConnData->iSocket != -1)
            {
                close(m_pstCurConnData->iSocket);
                m_pstCurConnData->iSocket = -1;
            }
            break;
        }
        case STCP_CONN_STATUS_E_SEND:
        {
            m_pstCurConnStatus->uiSendErrNum++;
            if (m_pstCurConnData->iSocket != -1)
            {
                close(m_pstCurConnData->iSocket);
                m_pstCurConnData->iSocket = -1;
            }
            break;
        }
        default:
        {
            break;
        }
    }
    m_pstCurConnData->tLastUseTime = time(NULL);

    return;
}


/**
 * @brief 发送数据
 */
int CSafeTcpClient::Send(const void *pszBuff, unsigned int uiLen)
{
    int iRetVal = 0;

    if (NULL == pszBuff)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "send fail: send buffer is NULL");
        return -1;
    }

    if (uiLen <= 0)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "send fail: send_len[%d] is not valid", uiLen);
        return -1;
    }

    //1.获取一个有效连接，包括ConnData和ConnStatus指针，分别存储到成员变量里面
    iRetVal = GetConnection();
    if (iRetVal != 0)
    {
        return -1;
    }

    //2.发送数据
    iRetVal = SendInternal(pszBuff, uiLen);
    if (iRetVal != 0)
    {
        return -1;
    }

    //3.设置连接状态
    SetSTcpConnStatusInternal(STCP_CONN_STATUS_OK);

    return 0;
}

/**
 * @brief 接收数据
 */
int CSafeTcpClient::Recv(void *pszBuff, unsigned int *puiLen, unsigned int uiExpectLen/* = 0*/)
{
    int iRetVal = 0;

    if (NULL == pszBuff)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "recv fail: recv buffer is NULL");
        return -1;
    }

    if ((*puiLen <= 0)||((uiExpectLen > 0)&&(uiExpectLen > *puiLen)))
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "recv fail: recv_buffer_len[%d] expect_len[%d] is not valid", *puiLen, uiExpectLen);
        return -1;
    }

    //1.获取一个有效连接，包括ConnData和ConnStatus指针，分别存储到成员变量里面
    //iRetVal = GetConnection();
    //if (iRetVal != 0)
    //{
    //    return -1;
    //}
    //RECV不能随便找一个连接收数据，必须是上次发送数据的那个连接

    //2.接收数据
    iRetVal = RecvInternal(pszBuff, puiLen, uiExpectLen);
    if (iRetVal != 0)
    {
        return -1;
    }

    //3.设置连接状态
    SetSTcpConnStatusInternal(STCP_CONN_STATUS_OK);

    return 0;

}

/**
 * @brief 发送并接收数据
 */
int CSafeTcpClient::SendAndRecv(const void * pszSendBuff, unsigned int uiSendLen, void * pszRecvBuff, unsigned int *puiRecvLen, unsigned int uiExpectLen /*= 0*/)
{
    int iRetVal = 0;

    if ((NULL == pszSendBuff)||(NULL == pszRecvBuff))
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "sendrecv fail: send buff or recv buffer is NULL");
        return -1;
    }

    if ((uiSendLen <= 0)||(*puiRecvLen <= 0)||((uiExpectLen > 0)&&(uiExpectLen > *puiRecvLen)))
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "sendrecv fail: sendlen[%d] recv_buffer_len[%d] expect_len[%d] is not valid", uiSendLen, *puiRecvLen, uiExpectLen);
        return -1;
    }

    //1.获取一个有效连接，包括ConnData和ConnStatus指针，分别存储到成员变量里面
    iRetVal = GetConnection();
    if (iRetVal != 0)
    {
        return -1;
    }

    //2.发送数据
    iRetVal = SendInternal(pszSendBuff, uiSendLen);
    if (iRetVal != 0)
    {
        return -1;
    }

    //3.接收数据
    iRetVal = RecvInternal(pszRecvBuff, puiRecvLen, uiExpectLen);
    if (iRetVal != 0)
    {
        return -1;
    }

    //4.设置连接状态
    SetSTcpConnStatusInternal(STCP_CONN_STATUS_OK);

    return 0;

}

/**
 * @brief 关闭当前连接
 */
void CSafeTcpClient::CloseCurConn()
{
    if (m_pstCurConnData == NULL)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "internal err:%s: conn_data is null", __func__);
    }
    else if (m_pstCurConnData->iSocket != -1)
    {
        close(m_pstCurConnData->iSocket);
        m_pstCurConnData->iSocket = -1;
    }

}

/**
 * @brief 关闭所有连接
 */
void CSafeTcpClient::CloseAllConn()
{
    //关闭所有连接
    for(int i=0; i<m_iSTcpConnDataNum; i++)
    {
        if (m_astSTcpConnData[i].iSocket != -1)
        {
            close(m_astSTcpConnData[i].iSocket);
            m_astSTcpConnData[i].iSocket = -1;
        }
    }
}


/**
 * @brief 关闭所有连接，并且Detach共享内存
 */
void CSafeTcpClient::Destory()
{
    //关闭所有连接
    CloseAllConn();

    //释放内存
    if (m_pstSTcpConnStatus != NULL)
    {
        m_objStatusMem.Detach();
        m_pstSTcpConnStatus = NULL;
        m_pStatusMem = NULL;
    }

    return;
}

/**
 * @brief 获取所有连接状态
 */
int CSafeTcpClient::GetAllStatus(STcpConnStatusHeader &stSTcpConnStatusHeader, std::vector<STcpConnStatus> &vstSTcpConnStatus)
{
    int iRetVal = 0;
    if (m_pstSTcpConnStatus == NULL)
    {
        iRetVal = InitStatusShm();
        if (iRetVal != 0)
        {
            return -1;
        }
    }

    memcpy(&stSTcpConnStatusHeader, m_pstSTcpConnStatus, sizeof(STcpConnStatusHeader));

    if (m_pstSTcpConnStatus->uiConnStatusNum > (unsigned int)STCP_MAX_CONN_STATUS)
    {
        m_pstSTcpConnStatus->uiConnStatusNum = STCP_MAX_CONN_STATUS;
    }

    vstSTcpConnStatus.clear();
    for (unsigned int i=0; i<m_pstSTcpConnStatus->uiConnStatusNum; i++)
    {
        vstSTcpConnStatus.push_back(m_pstSTcpConnStatus->astSTcpConnStatus[i]);
    }

    return 0;
}

/**
 * @brief 获取该API创建的各个连接状态
 */
int CSafeTcpClient::GetMyStatus(std::vector<STcpConnStatus> &vstSTcpConnStatus)
{
    if (m_pstSTcpConnStatus == NULL)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "internal err:%s: conn_status is null", __func__);
        return -1;
    }

    if (m_pstSTcpConnStatus->uiConnStatusNum > (unsigned int)STCP_MAX_CONN_STATUS)
    {
        m_pstSTcpConnStatus->uiConnStatusNum = STCP_MAX_CONN_STATUS;
    }

    vstSTcpConnStatus.clear();
    for (int i=0; i<m_iSTcpConnDataNum; i++)
    {
        for (unsigned int j=0; j<m_pstSTcpConnStatus->uiConnStatusNum; j++)
        {
            if ((m_pstSTcpConnStatus->astSTcpConnStatus[j].uiIPAddr == m_astSTcpConnData[i].uiIPAddr)
                &&(m_pstSTcpConnStatus->astSTcpConnStatus[j].ushPort == m_astSTcpConnData[i].ushPort))
            {
                vstSTcpConnStatus.push_back(m_pstSTcpConnStatus->astSTcpConnStatus[j]);
            }
        }
    }

    return 0;
}

/**
 * @brief 设置某个IP/端口对应的状态
 */
int CSafeTcpClient::SetConnStatus(const STcpConnStatus &stSTcpConnStatus)
{
    int iRetVal = 0;
    if (m_pstSTcpConnStatus == NULL)
    {
        iRetVal = InitStatusShm();
        if (iRetVal != 0)
        {
            return -1;
        }
    }

    m_pstCurConnStatus = AddSTcpConnStatus(stSTcpConnStatus.uiIPAddr, stSTcpConnStatus.ushPort);

    SetSTcpConnStatusInternal(stSTcpConnStatus.ushStatus);

    return 0;
}


/**
 * @brief 删除某个IP/端口对应的状态
 */
int CSafeTcpClient::DelConnStatus(const STcpConnStatus &stSTcpConnStatus)
{
    int iRetVal = 0;
    if (m_pstSTcpConnStatus == NULL)
    {
        iRetVal = InitStatusShm();
        if (iRetVal != 0)
        {
            return -1;
        }
    }

    DelConnStatusInternal(stSTcpConnStatus.uiIPAddr, stSTcpConnStatus.ushPort);

    return 0;
}

