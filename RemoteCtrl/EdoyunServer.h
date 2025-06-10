#pragma once
#include <MSWSock.h>
#include "EdoyunThread.h"
#include "EdoyunQueue.h"
#include <map>

#include <WS2tcpip.h> // Windows 平台需要包含此头文件
#pragma comment(lib, "ws2_32.lib") // 链接库


enum EdoyunOperator {
    ENone,
    EAccept,
    ERecv,
    ESend,
    EError
};

class EdoyunServer;
class EdoyunClient;
typedef std::shared_ptr<EdoyunClient> PCLIENT;




class EdoyunOverlapped {
public:
    OVERLAPPED m_overlapped;
    DWORD m_operator;//操作 参见EdoyunOperator
    std::vector<char> m_buffer;//缓冲区
    ThreadWorker m_worker;//处理函数
    EdoyunServer* m_server;//服务器对象
    EdoyunClient *m_client;//对应的客户端
    WSABUF m_wsabuffer;
    virtual ~EdoyunOverlapped() {
        m_buffer.clear();
    }

};

template<EdoyunOperator> class AcceptOverlapped;
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;

template<EdoyunOperator> class RecvOverlapped;
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;

template<EdoyunOperator> class SendOverlapped;
typedef SendOverlapped<ESend> SENDOVERLAPPED;



class EdoyunClient: public ThreadFuncBase {
public:
    EdoyunClient();

    ~EdoyunClient() {
        m_buffer.clear();
        closesocket(m_sock);
        m_recv.reset();
        m_send.reset();
        m_overlapped.reset();
        m_vecSend.Clear();
    }

    void SetOverlapped(PCLIENT& ptr);

    operator SOCKET() {
        return m_sock;
    }
    operator PVOID() {
        return &m_buffer[0];
    }
    operator LPOVERLAPPED();

    operator LPDWORD() {
        return &m_received;
    }
    LPWSABUF RecvWSABuffer();
    LPWSABUF SendWSABuffer();
    DWORD& flags(){return m_flags;}
    sockaddr_in* GetLocalAddr() { return &m_laddr;}
    sockaddr_in* GetRemoteAddr() { return &m_raddr;}
    size_t GetBufferSize()const {return m_buffer.size();}
    int Recv();
    int Send(void* buffer, size_t nSize);
    int SendData(std::vector<char>& data);
private:
    SOCKET m_sock;
    DWORD m_received;
    DWORD m_flags;
    std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;
    std::shared_ptr<RECVOVERLAPPED> m_recv;
    std::shared_ptr<SENDOVERLAPPED> m_send;
    std::vector<char> m_buffer;
    size_t m_used;//已使用的缓冲区大小 
    sockaddr_in m_laddr;
    sockaddr_in m_raddr;
    bool m_isbusy;
    EdoyunSendQueue<std::vector<char>> m_vecSend;  //发送数据队列
};



template<EdoyunOperator>
class AcceptOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
    AcceptOverlapped();
    int AcceptWorker();
};


template<EdoyunOperator>
class RecvOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
    RecvOverlapped();
    int RecvWorker() {
       int ret = m_client->Recv();
       return ret;
    }
};

template<EdoyunOperator>
class SendOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
    SendOverlapped();
    int SendWorker() {
        //TODO:
        return -1;
    }
};
typedef SendOverlapped<ESend> SENDOVERLAPPED;

template<EdoyunOperator>
class ErrorOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
    ErrorOverlapped() :m_operator(EError), m_worker(this, &ErrorOverlapped::ErrorWorker) {
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        m_buffer.resize(1024);
    }
    int ErrorWorker() {
        //TODO:
        return -1;

    }
};
typedef ErrorOverlapped<EError> ERROROVERLAPPED;




class EdoyunServer : public ThreadFuncBase
{
public:
    EdoyunServer(const std::string& ip = "0.0.0.0", short port = 9527) :m_pool(10) {
        m_hIOCP = INVALID_HANDLE_VALUE;
        m_sock = INVALID_SOCKET;
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = htons(port);
        //m_addr.sin_addr.s_addr = inet_addr(ip.c_str());// 已弃用
        inet_pton(AF_INET, ip.c_str(), &m_addr.sin_addr);
    }
    ~EdoyunServer();

    bool startService();

    bool NewAccept() {
        PCLIENT pClient(new EdoyunClient());
        pClient->SetOverlapped(pClient);
        m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));
        if (!AcceptEx(m_sock,
            *pClient,
            *pClient,
            0,
            sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
            *pClient, *pClient))
        {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            m_hIOCP = INVALID_HANDLE_VALUE;
            return false;
        }
        return true;
    }

private:
    void CreateSocket() {
        m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
        int opt = 1;
        setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    }

    
    int threadIocp();

private:
    EdoyunThreadPool m_pool;
    HANDLE m_hIOCP;
    SOCKET m_sock;
    std::map<SOCKET, std::shared_ptr<EdoyunClient>> m_client;
    sockaddr_in m_addr;
};

typedef EdoyunSendQueue<std::vector<char>>::EDYCALLBACK SENDCALLBACK;

