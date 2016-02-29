#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/poll.h>
#include <arpa/inet.h>

#include "tcp_client/tcp_client.h"

using namespace mmlib;

CTcpClient::CTcpClient(): m_iServerPort(0), m_iTimeOut(TCP_DEFAULT_TIMEOUT),m_iSocket(0), m_bSocketInited(false)
{
    memset(m_szErrMsg, 0, sizeof(m_szErrMsg));
    memset(m_szServerIP, 0, sizeof(m_szServerIP));
    memset(&m_stSockAddr, 0, sizeof(m_stSockAddr));
    m_iCheckConnect=-1;
}

CTcpClient::~CTcpClient()
{
    CloseSocket();
}

int CTcpClient::SetTimeOut(int iTimeOut)
{
    m_iTimeOut = iTimeOut;

    return 0;
}

int CTcpClient::Init(const char * pszIP, int iPort, int iTimeout)
{
    int nRet(0);

    snprintf(m_szServerIP, sizeof(m_szServerIP), "%s", pszIP);
    m_iServerPort = iPort;
    m_iTimeOut = iTimeout;

    nRet=ConnectServer();
    if(nRet==0)
    {
        m_iCheckConnect=0;
    }
    return nRet;
}

int CTcpClient::InitSocket()
{
    if (!m_bSocketInited)
    {
        m_iSocket = socket(PF_INET, SOCK_STREAM, 0);
        if (-1 == m_iSocket)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", strerror(errno));
            return -1;
        }
        m_bSocketInited = true;
    }

    return 0;
}

void CTcpClient::CloseSocket()
{
    if (m_bSocketInited)
    {
        close(m_iSocket);
        m_bSocketInited = false;
    }
}

int CTcpClient::ConnectServer()
{
    if (0 != InitSocket())
        return -1;

    m_stSockAddr.sin_family = AF_INET;
    m_stSockAddr.sin_port = htons(m_iServerPort);
    m_stSockAddr.sin_addr.s_addr = inet_addr(m_szServerIP);

    if (-1 == connect(m_iSocket, (struct sockaddr *)&m_stSockAddr, sizeof(m_stSockAddr)))
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "errno=%d:%s", errno,strerror(errno));
        return errno;
    }

    return 0;
}

int CTcpClient::ReconnectServer()
{
    CloseSocket();
    int nRet = ConnectServer();
    if (nRet == 0)
    {
        m_iCheckConnect = 0;
    }

    return nRet;
}

int CTcpClient::RecvWait(int iTimeOut)
{
    return PollWait(POLLIN, iTimeOut);
}

int CTcpClient::SendWait(int iTimeOut)
{
    return PollWait(POLLOUT, iTimeOut);
}

int CTcpClient::PollWait(short shEvent, int iTimeOut)
{
    struct pollfd stPoll;

    stPoll.fd = m_iSocket;
    stPoll.events = shEvent;
    stPoll.revents = 0;

    while(true)
    {
        switch (poll(&stPoll, 1, iTimeOut))
        {
        case -1:
            if (errno != EINTR)
            {
                snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", strerror(errno));
                return -1;
            }
            continue;
        case 0:
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "poll timeout");
            return -1;

        default:
            if (stPoll.revents & POLLHUP)
            {
                snprintf(m_szErrMsg, sizeof(m_szErrMsg), "poll returned POLLHUP.");
                return -1;
            }
            if (stPoll.revents & POLLERR)
            {
                snprintf(m_szErrMsg, sizeof(m_szErrMsg), "poll returned POLLERR.");
                return -1;
            }
            if (stPoll.revents & POLLNVAL)
            {
                snprintf(m_szErrMsg, sizeof(m_szErrMsg), "poll returned POLLNVAL.");
                return -1;
            }
            if (stPoll.revents & shEvent)
            {
                return 0;
            }
            else
            {
                snprintf(m_szErrMsg, sizeof(m_szErrMsg), "poll returned event error.");
                return -1;
            }
        }
    }
}

int CTcpClient::Send(void * pabyBuffer, int iLength)
{
    unsigned int uiLength = iLength;
    return Send(pabyBuffer, uiLength);
}

int CTcpClient::Send(void * pabyBuffer, unsigned int uiLength)
{
    if (NULL == pabyBuffer)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "Send error: send buffer is NULL");
        return -1;
    }

    unsigned int uiBytesSent = 0;
    int iWriteBytes = 0;

    while(true)
    {
        if (0 != SendWait(m_iTimeOut))
        {
            m_iCheckConnect=-1;
            return -1;
        }

        iWriteBytes = write(m_iSocket, (unsigned char *)pabyBuffer + uiBytesSent, uiLength - uiBytesSent);
        if (iWriteBytes > 0)
        {
            uiBytesSent += iWriteBytes;
            if (uiBytesSent < uiLength) continue;
            else break;
        }
        else if (errno == EINTR) continue;
        else break;
    }

    if (iWriteBytes < 0)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "Send error: %s", strerror(errno));
        m_iCheckConnect=-1;
        return -1;
    }

    return 0;
}

int CTcpClient::Recv(void * pabyBuffer, int &iLength, unsigned int uiExpectLen)
{
    int iRetVal = 0;
    unsigned int uiLength = iLength;
    iRetVal = Recv(pabyBuffer, uiLength, uiExpectLen);
    iLength = uiLength;

    return iRetVal;
}

int CTcpClient::Recv(void * pabyBuffer, unsigned int &uiLength, unsigned int uiExpectLen)
{
    if (NULL == pabyBuffer)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "Send error: recv buffer is NULL");
        return -1;
    }

    unsigned int uiBytesReceived = 0;
    int iReadBytes = 0;

    while (true)
    {
        if (0 != RecvWait(m_iTimeOut))
        {
            m_iCheckConnect=-1;
            return -1;
        }

        if (uiExpectLen > 0)
        {
            iReadBytes = read(m_iSocket, (unsigned char *)pabyBuffer + uiBytesReceived, uiExpectLen - uiBytesReceived);
            if (iReadBytes > 0)
            {
                uiLength = (uiBytesReceived += iReadBytes);
                if (uiBytesReceived < uiExpectLen) continue;
                else break;
            }
            else if (errno == EINTR) continue;
            else break;
        }
        else
        {
            iReadBytes = read(m_iSocket, pabyBuffer, uiLength);
            if (iReadBytes > 0)
            {
                uiLength = iReadBytes;
                return 0;
            }
            else if (errno == EINTR) continue;
            else break;
        }
    }

    if (iReadBytes <= 0)
    {
        m_iCheckConnect=-1;
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "Recv error: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int CTcpClient::SendAndRecv(void * pabyRequest, int iRequestLen, void * pabyResponse, int &iResponseLen)
{
    if (0 != Send(pabyRequest, iRequestLen))
    {
        m_iCheckConnect=-1;
        return -1;
    }

    if (0 != Recv(pabyResponse, iResponseLen))
    {
        m_iCheckConnect=-1;
        return -2;
    }

    return 0;
}

int CTcpClient::SendAndRecv(void * pabyRequest, unsigned int uiRequestLen, void * pabyResponse, unsigned int &uiResponseLen)
{
    if (0 != Send(pabyRequest, uiRequestLen))
    {
        m_iCheckConnect=-1;
        return -1;
    }

    if (0 != Recv(pabyResponse, uiResponseLen))
    {
        m_iCheckConnect=-1;
        return -2;
    }

    return 0;
}


