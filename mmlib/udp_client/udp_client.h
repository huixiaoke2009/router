#ifndef UDPCLIENT_H
#define UDPCLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace mmlib
{

class CUdpClient
{
public:
    CUdpClient();
    ~CUdpClient();

    const static int UDP_DEFAULT_TIMEOUT = 5000;        // UDP Client 超时默认值
    int SetTimeOut(int iTimeOut);
    int Connect(const char * pszIP, int iPort);
    int Send(unsigned char * pabyBuffer, unsigned int uiLength);
    int Recv(unsigned char * pabyBuffer, unsigned int &uiLength);
    int SendAndRecv(unsigned char * pabyRequest, unsigned int uiRequestLen, unsigned char * pabyResponse, unsigned int &uiResponseLen);
    void Close()
    {
        CloseSocket();
    }
    inline const char *GetErrMsg()
    {
        return m_szErrMsg;
    }
private:
    char m_szErrMsg[1024];
    int m_iTimeOut;
    char m_szServerIP[16];
    int m_iServerPort;
    int m_iSocket;
    struct sockaddr_in m_stSockAddr;
    bool m_bSocketInited;

    int InitSocket();
    void CloseSocket();
};

}
#endif

