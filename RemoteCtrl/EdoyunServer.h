#pragma once
#include <MSWSock.h>
#include "EdoyunThread.h"
#include "EdoyunQueue.h"
#include <map>

#include <WS2tcpip.h> // Windows 平台需要包含此头文件
#pragma comment(lib, "ws2_32.lib") // 链接库


//标识重叠I/O操作的具体类型
enum EdoyunOperator {
    ENone,
    EAccept,
    ERecv,
    ESend,
    EError
};

// 前向声明EdoyunServer和EdoyunClient类，避免循环依赖
class EdoyunServer;
class EdoyunClient;
// 定义PCLIENT作为EdoyunClient的共享指针类型，方便资源管理
typedef std::shared_ptr<EdoyunClient> PCLIENT;


// 基础重叠I / O结构
class EdoyunOverlapped {
public:
    OVERLAPPED m_overlapped;      // Windows重叠I/O结构 第一个成员必须是OVERLAPPED类型
    DWORD m_operator;             // 操作类型(对应EdoyunOperator)
    std::vector<char> m_buffer;   // 数据缓冲区
    ThreadWorker m_worker;        // 封装的处理函数
    EdoyunServer* m_server;       // 关联的服务器对象
    EdoyunClient* m_client;       // 关联的客户端对象
    WSABUF m_wsabuffer;           // Windows套接字缓冲区结构 用于Winsock API

    virtual ~EdoyunOverlapped() {
        m_buffer.clear();
    }
};

// 通过模板特化来创建特定操作类型的重叠I/O结构
// 接受连接的重叠I / O结构
template<EdoyunOperator> class AcceptOverlapped;
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;

//接收数据的重叠I / O结构
template<EdoyunOperator> class RecvOverlapped;
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;

//发送数据的重叠I / O结构
template<EdoyunOperator> class SendOverlapped;
typedef SendOverlapped<ESend> SENDOVERLAPPED;


// 作为客户端连接的核心封装类，管理单个连接的生命周期
class EdoyunClient: public ThreadFuncBase {
public:
    EdoyunClient();

    ~EdoyunClient() {
        m_buffer.clear();          // 清空数据缓冲区
        closesocket(m_sock);       // 关闭套接字
        m_recv.reset();            // 释放 接收重叠结构
        m_send.reset();            // 释放 发送重叠结构
        m_overlapped.reset();      // 释放 基础重叠结构
        m_vecSend.Clear();         // 清空 发送队列
    }

    // 设置重叠I/O结构  PCLIENT就是EdoyunClient的共享智能指针
    void SetOverlapped(PCLIENT& ptr);

    // 转换操作符 转换为对应的类型，提供隐式转换能力，方便与Windows API交互
    operator SOCKET() {return m_sock;}//获取底层套接字句柄
    operator PVOID() {return &m_buffer[0];}//获取缓冲区起始地址
    operator LPOVERLAPPED();//转换为重叠结构指针
    operator LPDWORD() {return &m_received;}//获取接收字节数指针

    // WSABUF相关函数
    LPWSABUF RecvWSABuffer();
    LPWSAOVERLAPPED RecvOverlapped();
    LPWSABUF SendWSABuffer();
    LPWSAOVERLAPPED SendOverlapped();

    //辅助函数
    DWORD& flags(){return m_flags;}
    sockaddr_in* GetLocalAddr() { return &m_laddr;}
    sockaddr_in* GetRemoteAddr() { return &m_raddr;}
    size_t GetBufferSize()const {return m_buffer.size();}

    //​​​​网络操作函数
    int Recv();
    int Send(void* buffer, size_t nSize);
    int SendData(std::vector<char>& data);
private:
    SOCKET m_sock;                  // 客户端套接字句柄
    DWORD m_received;               // 已接收字节数
    DWORD m_flags;                  // 操作标志位
    std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;  // 接受连接重叠结构
    std::shared_ptr<RECVOVERLAPPED> m_recv;          // 接收数据重叠结构
    std::shared_ptr<SENDOVERLAPPED> m_send;          // 发送数据重叠结构
    std::vector<char> m_buffer;     // 数据缓冲区
    size_t m_used;                  // 已使用的缓冲区大小
    sockaddr_in m_laddr;            // 本地地址信息
    sockaddr_in m_raddr;            // 远程地址信息
    bool m_isbusy;                  // 忙碌状态标志
    EdoyunSendQueue<std::vector<char>> m_vecSend;  // 发送数据队列
};


// 模板类封装重叠I / O结构
template<EdoyunOperator>
class AcceptOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
    AcceptOverlapped();
    int AcceptWorker();//接受连接的工作函数
};

template<EdoyunOperator>
class RecvOverlapped :public EdoyunOverlapped, ThreadFuncBase
{
public:
    RecvOverlapped();
    // 接收数据的工作函数，调用客户端的Recv()
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
    // 发送数据的工作函数(待实现)
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
    // 错误处理的工作函数(待实现)
    int ErrorWorker() {
        //TODO:
        return -1;

    }
};
typedef ErrorOverlapped<EError> ERROROVERLAPPED;


// 基于IOCP的服务器框架 继承自ThreadFuncBase（在EdoyunThread.h中定义的空基类）
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

    //服务器控制函数
    bool startService();//启动服务
    bool NewAccept();//创建新的接受连接操作
    void BindNewSocket(SOCKET s);//绑定新socket到IOCP

private:
    void CreateSocket();// 创建服务器socket
    int threadIocp();// IOCP工作线程函数

private:
    EdoyunThreadPool m_pool;
    HANDLE m_hIOCP;
    SOCKET m_sock;
    std::map<SOCKET, std::shared_ptr<EdoyunClient>> m_client;
    sockaddr_in m_addr;
};

// 定义发送数据回调函数类型
typedef EdoyunSendQueue<std::vector<char>>::EDYCALLBACK SENDCALLBACK;

