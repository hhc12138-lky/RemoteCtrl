#include "pch.h"
#include "EdoyunServer.h"
#include "EdoyunTool.h"
#pragma warning(disable: 4407)

template<EdoyunOperator op>
AcceptOverlapped<op>::AcceptOverlapped(){
    // 初始化工作线程函数
    m_worker = ThreadWorker(this, (FUNCTYPE)&AcceptOverlapped<op>::AcceptWorker);
    m_operator = EAccept; // 设置操作类型
    memset(&m_overlapped, 0, sizeof(m_overlapped)); //  初始化 overlapped 结构体
    m_buffer.resize(1024); // 预分配缓冲区
    m_server = NULL; // 初始化服务器指针
}

// 处理连接接受的核心工作函数
template<EdoyunOperator op>
int AcceptOverlapped<op>::AcceptWorker()
{
    INT lLength = 0, rLength = 0;
    if (m_client->GetBufferSize() > 0) { // 缓冲区大小检查
        // 解析地址信息
        sockaddr* plocal = NULL, * premote = NULL;
        GetAcceptExSockaddrs(*m_client, 0,
            sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
            (sockaddr**)&plocal, &lLength, //本地地址
            (sockaddr**)&premote, &rLength  //远程地址
        );
        // 将解析出的地址复制到客户端对象中
        memcpy(m_client->GetLocalAddr(), plocal, sizeof(sockaddr_in));
        memcpy(m_client->GetRemoteAddr(), premote, sizeof(sockaddr_in));
        // 绑定新socket到IOCP
        m_server->BindNewSocket(*m_client);
        // 调用WSARecv开始异步数据接收
        int ret = WSARecv((SOCKET)*m_client, m_client->RecvWSABuffer(), 1, *m_client, &m_client->flags(), m_client->RecvOverlapped(), NULL);
        if (ret == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING)) {
            //TODO:报错
            TRACE("ret = %d error = %d\n", ret, WSAGetLastError());
        }
        // 继续接受新连接 保持服务器持续监听
        if (!m_server->NewAccept()) {
            return -2;
        }
    }
    return -1;
}


template<EdoyunOperator op>
inline SendOverlapped<op>::SendOverlapped() {
    m_operator = op;  // 设置操作类型为模板参数op
    m_worker = ThreadWorker(this, (FUNCTYPE)&SendOverlapped<op>::SendWorker);  // 初始化工作线程函数
    memset(&m_overlapped, 0, sizeof(m_overlapped));  // 清零重叠I/O结构
    m_buffer.resize(1024 * 256);  // 分配256KB发送缓冲区
}

template<EdoyunOperator op>
inline RecvOverlapped<op>::RecvOverlapped() {
    m_operator = op;
    m_worker = ThreadWorker(this, (FUNCTYPE)&RecvOverlapped<op>::RecvWorker);
    memset(&m_overlapped, 0, sizeof(m_overlapped));
    m_buffer.resize(1024 * 256);
}


EdoyunClient::EdoyunClient()
    :m_isbusy(false), m_flags(0), 
    m_overlapped(new ACCEPTOVERLAPPED()),
    m_recv(new RECVOVERLAPPED()),
    m_send(new SENDOVERLAPPED()),
    m_vecSend(this,(SENDCALLBACK)& EdoyunClient::SendData)
{
    m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    m_buffer.resize(1024);
    memset(&m_laddr, 0, sizeof(m_laddr));
    memset(&m_raddr, 0, sizeof(m_raddr));
}

void EdoyunClient::SetOverlapped(PCLIENT& ptr) {
    // 设置自身成员变量为自己 使得在IOCP完成回调中可以通过OVERLAPPED结构获取对应的客户端对象
    m_overlapped->m_client = ptr.get();
    m_recv->m_client = ptr.get();
    m_send->m_client = ptr.get();
}

EdoyunClient::operator LPOVERLAPPED() {
    return &m_overlapped->m_overlapped;
}

LPWSABUF EdoyunClient::RecvWSABuffer()
{
    return &m_recv->m_wsabuffer;
}

LPWSAOVERLAPPED EdoyunClient::RecvOverlapped()
{
    return &m_recv->m_overlapped;
}

LPWSABUF EdoyunClient::SendWSABuffer()
{
    return &m_send->m_wsabuffer;
}

LPWSAOVERLAPPED EdoyunClient::SendOverlapped()
{
    return &m_send->m_overlapped;
}

int EdoyunClient::Recv()
{
    int ret = recv(m_sock, m_buffer.data() + m_used, m_buffer.size() - m_used, 0);
    if (ret <= 0) return -1;
    m_used += (size_t)ret;
    CEdoyunTool::Dump((BYTE*)m_buffer.data(), ret);
    return 0;
}

int EdoyunClient::Send(void* buffer, size_t nSize)
{
    std::vector<char> data(nSize);
    memcpy(data.data(), buffer, nSize);
    /*if (m_vecSend.PushBack(data)) {
        return 0;
    }
    return -1;*/
    return 0;
}

int EdoyunClient::SendData(std::vector<char>& data)
{
    if (m_vecSend.Size() > 0) { // 检查发送队列是否有数据
        // 使用WSASend发起非阻塞发送
        int ret = WSASend(m_sock, SendWSABuffer(), 1, &m_received, m_flags, &m_send->m_overlapped, NULL);

        if (ret != 0 && (WSAGetLastError() != WSA_IO_PENDING)) {
            CEdoyunTool::ShowError();
            return -1;
        }
    }
    return 0;
}

EdoyunServer::~EdoyunServer()
{
    closesocket(m_sock);
    // 遍历 std::map<SOCKET, PCLIENT> 容器并重置所有客户端资源
    std::map<SOCKET, PCLIENT>::iterator it = m_client.begin();
    for (; it != m_client.end(); it++) {
        it->second.reset();  // 对每个客户端调用 reset() 方法
    }
    m_client.clear();  // 清空整个容器
    CloseHandle(m_hIOCP);
    m_pool.Stop();
    WSACleanup();
}

bool EdoyunServer::startService()
{
    // 创建服务器套接字
    CreateSocket();

    // 绑定 监听
    if (bind(m_sock, (sockaddr*)&m_addr, sizeof(m_addr)) == -1) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }
    if (listen(m_sock, 3) == -1) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }

    // 创建IOCP完成端口
    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);
    if (m_hIOCP == NULL) {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        m_hIOCP = INVALID_HANDLE_VALUE;
        return false;
    }
    // 关联套接字到IOCP
    CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);
    // 启动线程池
    m_pool.Invoke();
    m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&EdoyunServer::threadIocp));
    // 投递初始接受请求
    if (!NewAccept()) return false;
    //m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&EdoyunServer::threadIocp));
    //m_pool.DispatchWorker(ThreadWorker(this!=(FUNCTYPE)&EdoyunServer::threadIocp));
    return true;
}

// ​​IOCP异步接受新连接
bool EdoyunServer::NewAccept()
{
    // 创建客户端对象
    PCLIENT pClient(new EdoyunClient());
    // 初始化重叠结构
    pClient->SetOverlapped(pClient);
    // 存储客户端引用
    m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));
    // 投递异步接受请求
    if (!AcceptEx(m_sock,     // 监听套接字
        *pClient,    // 接受的客户端套接字
        *pClient,    // 接收缓冲区(此处复用socket转换 根据PVOID类型转换)
        0,           // 接收数据长度(0表示不接收)
        sizeof(sockaddr_in) + 16,  // 本地地址信息长度
        sizeof(sockaddr_in) + 16,  // 远程地址信息长度
        *pClient,    // 接收字节数(复用转换)
        *pClient))   // 重叠结构(复用转换)
    {
        // 错误处理
        if (WSAGetLastError() != WSA_IO_PENDING) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            m_hIOCP = INVALID_HANDLE_VALUE;
            return false;
        }
    }
    return true;
}

void EdoyunServer::BindNewSocket(SOCKET s)
{
    CreateIoCompletionPort((HANDLE)s, m_hIOCP, (ULONG_PTR)this, 0);
}

void EdoyunServer::CreateSocket()
{
    // 初始化Winsock DLL，必须在其他socket函数前调用
    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 2), &WSAData);
    // 创建套接字
    m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    // 设置套接字选项 允许服务器快速重启
    int opt = 1;
    setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
}

int EdoyunServer::threadIocp()
{
    DWORD tranferred = 0;
    ULONG_PTR CompletionKey = 0;
    OVERLAPPED* lpOverlapped = NULL;
    // 完成端口等待
    if (GetQueuedCompletionStatus(m_hIOCP, &tranferred, &CompletionKey, &lpOverlapped, INFINITE)) {
        if (CompletionKey != 0) {
            // 通过结构体成员指针获取父结构体指针
            EdoyunOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, EdoyunOverlapped, m_overlapped);
            pOverlapped->m_server = this;

            // 操作类型分发
            switch (pOverlapped->m_operator) {
            case EAccept:
            {
                ACCEPTOVERLAPPED* pOver = (ACCEPTOVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;
            case ERecv:
            {
                RECVOVERLAPPED* pOver = (RECVOVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;
            case ESend:
            {
                SENDOVERLAPPED* pOver = (SENDOVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;
            case EError:
            {
                ERROROVERLAPPED* pOver = (ERROROVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;
            }
        }
        else {
            return -1;
        }
    }
    return 0;
}
