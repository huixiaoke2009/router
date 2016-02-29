#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace mmlib
{

class CTcpClient
{
public:
    CTcpClient();
    ~CTcpClient();

    const static int TCP_DEFAULT_TIMEOUT = 5000;        // TCP Client 超时默认值
    int Init(const char * pszIP, int iPort, int iTimeout = TCP_DEFAULT_TIMEOUT);
    int SetTimeOut(int iTimeOut);

    int Send(void * pabyBuffer, unsigned int uiLength);
    int Send(void * pabyBuffer, int iLength);

    int Recv(void * pabyBuffer, unsigned int &uiLength, unsigned int uiExpectLen = 0);
    int Recv(void * pabyBuffer, int &iLength, unsigned int uiExpectLen = 0);

    int SendAndRecv(void * pabyRequest, int iRequestLen, void * pabyResponse, int &iResponseLen);
    int SendAndRecv(void * pabyRequest, unsigned int uiRequestLen, void * pabyResponse, unsigned int &uiResponseLen);

    int ReconnectServer();

    int CheckConnect()
    {
        //test code
        //m_iCheckConnect=-1;
        return m_iCheckConnect;
    }
    void Close()
    {
        CloseSocket();
    }
    void CloseSocket();
    const char *GetErrMsg()
    {
        return m_szErrMsg;
    }
public:
    char m_szServerIP[16];
    int m_iServerPort;
private:
    char m_szErrMsg[1024];
    int m_iTimeOut;
    int m_iSocket;
    struct sockaddr_in m_stSockAddr;
    bool m_bSocketInited;

    int  m_iCheckConnect;

    int InitSocket();
    int ConnectServer();
    int PollWait(short shEvent, int iTimeOut);
    int RecvWait(int iTimeOut);
    int SendWait(int iTimeOut);
};

}
#endif
