#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "udp_client/udp_client.h"

using namespace mmlib;

CUdpClient::CUdpClient(): m_iTimeOut(UDP_DEFAULT_TIMEOUT), m_iServerPort(0), m_iSocket(0),
    m_bSocketInited(false)
{
    memset(m_szErrMsg, 0, sizeof(m_szErrMsg));
    memset(m_szServerIP, 0, sizeof(m_szServerIP));
    memset(&m_stSockAddr, 0, sizeof(m_stSockAddr));
}

CUdpClient::~CUdpClient()
{
    CloseSocket();
}

int CUdpClient::SetTimeOut(int iTimeOut)
{
    m_iTimeOut = iTimeOut;
    return 0;
}

int CUdpClient::Connect(const char * pszIP, int iPort)
{
    CloseSocket();
    if (0 != InitSocket())
        return -1;

    snprintf(m_szServerIP, sizeof(m_szServerIP), "%s", pszIP);
    m_iServerPort = iPort;

    m_stSockAddr.sin_family = AF_INET;
    m_stSockAddr.sin_port = htons(m_iServerPort);
    m_stSockAddr.sin_addr.s_addr = inet_addr(m_szServerIP);

    if (-1 == connect(m_iSocket, (struct sockaddr *)&m_stSockAddr, sizeof(m_stSockAddr)))
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", strerror(errno));
        return -1;
    }

    return 0;
}

int CUdpClient::InitSocket()
{
    int iRetVal = 0;
    if (!m_bSocketInited)
    {
        m_iSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (-1 == m_iSocket)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "%s", strerror(errno));
            return -1;
        }

        m_bSocketInited = true;

        struct timeval stTimeout;
        stTimeout.tv_sec = m_iTimeOut / 1000;
        stTimeout.tv_usec = (m_iTimeOut % 1000) * 1000;

        iRetVal = setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, &stTimeout, sizeof(stTimeout));
        if (iRetVal != 0)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "setsockopt failed, errmsg=%s", strerror(errno));
            return -1;
        }

        iRetVal = setsockopt(m_iSocket, SOL_SOCKET, SO_SNDTIMEO, &stTimeout, sizeof(stTimeout));
        if (iRetVal != 0)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "setsockopt failed, errmsg=%s", strerror(errno));
            return -1;
        }
    }

    return 0;
}

void CUdpClient::CloseSocket()
{
    if (m_bSocketInited)
    {
        close(m_iSocket);
        m_bSocketInited = false;
    }
}

int CUdpClient::Send(unsigned char * pabyBuffer, unsigned int uiLength)
{
    if ((send(m_iSocket, pabyBuffer, uiLength, 0)) == -1)
    {
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "send failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

int CUdpClient::Recv(unsigned char * pabyBuffer, unsigned int &uiLength)
{
    int iRetCode = recv(m_iSocket, pabyBuffer, uiLength, 0);
    if (iRetCode == -1)
    {
        uiLength = 0;
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "recv failed: %s", strerror(errno));
        return -1;
    }
    else if (iRetCode == 0)
    {
        uiLength = 0;
        snprintf(m_szErrMsg, sizeof(m_szErrMsg), "recv failed: peer has performed an orderly shutdown");
        return -1;
    }
    else
    {
        uiLength = iRetCode;
    }

    return 0;
}

int CUdpClient::SendAndRecv(unsigned char * pabyRequest, unsigned int uiRequestLen, unsigned char * pabyResponse, unsigned int &uiResponseLen)
{
    if (0 != Send(pabyRequest, uiRequestLen))
    {
        return -1;
    }

    if (0 != Recv(pabyResponse, uiResponseLen))
    {
        return -1;
    }

    return 0;
}


