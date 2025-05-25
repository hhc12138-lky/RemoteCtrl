#include "pch.h"
#include "ClientSocket.h"

CClientSocket* CClientSocket::m_instance = NULL;
CClientSocket::CHelper CClientSocket::m_helper;

CClientSocket* pclient = CClientSocket::getInstance();

std::string GetErrInfo(int wsaErrCode)
{
	// 获取系统错误信息​：将Windows Socket错误码转换为可读的字符串描述
	std::string ret;
	LPVOID lpMsgBuf = NULL;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		wsaErrCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	ret = (char*)lpMsgBuf;
	LocalFree(lpMsgBuf);
	return ret;
}

void Dump(BYTE* pData, size_t nSize)
{
	// 将二进制数据按十六进制格式输出到调试器（DebugView等工具可见）
	std::string strOut;
	for (size_t i = 0; i < nSize; i++)
	{
		char buf[8] = "";
		if (i > 0 && (i % 16 == 0))strOut += "\n";
		snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
		strOut += buf;
	}
	strOut += "\n";
	OutputDebugStringA(strOut.c_str());
}

CClientSocket::CClientSocket(const CClientSocket& ss) {
	m_hThread = INVALID_HANDLE_VALUE;
	m_bAutoClose = ss.m_bAutoClose;
	m_sock = ss.m_sock;
	m_nIP = ss.m_nIP;
	m_nPort = ss.m_nPort;
	for (auto it = m_mapFunc.begin(); it != m_mapFunc.end(); it++) {
		m_mapFunc.insert(std::pair<UINT, MSGFUNC>(it->first, it->second));
	}
}

CClientSocket::CClientSocket() :m_nIP(INADDR_ANY), m_nPort(0), m_sock(INVALID_SOCKET), m_bAutoClose(true), m_hThread(INVALID_HANDLE_VALUE) {
	if (InitSockEnv() == FALSE) {
		MessageBox(NULL, _T("无法初始化套接字环境,请检查网络设置！"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
		exit(0);
	}

	// 创建专用于数据包处理于响应的线程 使用m_eventInvoke事件机制监控是否启动
	m_eventInvoke = CreateEvent(NULL, FALSE, FALSE, NULL);// 属性为null，自动重置为true，初始值为false，名称为null
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientSocket::threadEntry, this, 0, &m_nThreadID);
	if (WaitForSingleObject(m_eventInvoke, 100) == WAIT_TIMEOUT) {
		TRACE("网络消息处理线程启动失败! \r\n");
	}
	CloseHandle(m_eventInvoke);// 关闭事件句柄

	m_buffer.resize(BUFFER_SIZE);
	memset(m_buffer.data(), 0, BUFFER_SIZE);
	struct {
		UINT message;
		MSGFUNC func;
	}funcs[] = {
		{WM_SEND_PACK,&CClientSocket::SendPack},
	};
	for (auto& func : funcs) {
		m_mapFunc[func.message] = func.func;
	}
}
bool CClientSocket::InitSocket()
{
	/*初始化并连接Socket​*/

	// 关闭旧连接
	if (m_sock != INVALID_SOCKET)CloseSocket();
	// 创建Socket
	m_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (m_sock == -1)return false;
	//​​设置服务器地址
	sockaddr_in serv_adr;
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET; // 指定地址族为 IPv4（AF_INET）

	TRACE("addr %08X nIP %08X\r\n", inet_addr("127.0.0.1"), m_nIP);
	serv_adr.sin_addr.s_addr = htonl(m_nIP);
	serv_adr.sin_port = htons(m_nPort);

	if (serv_adr.sin_addr.s_addr == INADDR_NONE) {
		AfxMessageBox("指定的IP地址，不存在！");
		return false;
	}

	//建立连接
int ret = connect(m_sock, (sockaddr*)&serv_adr, sizeof(serv_adr));
if (ret == -1) {
	AfxMessageBox("连接失败!");
	TRACE("连接失败：%d %s\r\n", WSAGetLastError(), GetErrInfo(WSAGetLastError()).c_str());
	return false;
}
return true;
}

int CClientSocket::DealCommand()
{
	//解析指令并返回命令号​+缓存数据到 m_buffer
	/*
	原理解析：流式数据接收+滑动窗口
	1. 调用一次DealCommand最终仅仅得到一个CPacket，但是由于tcp包的大小（有效载荷）一般为1460，所以一个CPacket需要多个
	tcp包发送。所以需要while (true)结合recv多次接受。
	2. tcp在发送多个CPacket包时按照字节流发送，不区分CPacket包之间的边界，因此需要每次CPacket((BYTE*)buffer, len);尝试解包。
	3. 解包成功后，由于缓冲区后面还有“已经被接受但未解析”的tcp数据

	*/
	if (m_sock == -1)return -1;
	// 缓冲区准备 用一个char*指向缓冲区的起始地址
	char* buffer = m_buffer.data(); // TODO：多线程发送命令时可能会冲突 index、buffer等都不安全
	static size_t index = 0;
	while (true) {
		// 从Socket接收数据，存入缓冲区未使用部分
		size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
		// 当接收失败(len<=0)且缓冲区为空(index<=0)时返回错误；允许接收失败但缓冲区有数据的情况（继续处理缓冲数据）
		if (((int)len <= 0) && ((int)index <= 0)) {
			return -1;
		}

		// 数据累积 更新缓冲区数据长度
		index += len;
		// 尝试将缓冲区数据解析为CPacket对象
		len = index;
		m_packet = CPacket((BYTE*)buffer, len);
		// 解包成功 len = CPacket的size，且包数据存储在成员函数m_packet中；解包失败 len = 0。
		// 移动数据到buffer头部，然后直接返回命令号。让外层循环多次调用 DealCommand，直到缓冲区无完整包为止
		if (len > 0) {
			// 将未处理数据移动到缓冲区头部
			memmove(buffer, buffer + len, index - len);
			// 更新未处理数据长度
			index -= len;
			// 返回命令号
			return m_packet.sCmd;
		}
	}
	return -1;
}
bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed, WPARAM wParam)
{
	UINT nMode = isAutoClosed ? CSM_AUTOCLOSE : 0;
	std::string strOut;
	pack.Data(strOut);
	// PostThreadMessage相比于SendMessage，PostThreadMessage不会阻塞线程，也不关心发送后有没有送达；而SendMessage会阻塞线程，保证送达。
	// 为了实现跨线程的消息发送，这里把pack在堆上new了个对象，包装后发送，记得要在消息处理线程中，处理完成后delete掉
	PACKET_DATA* pData = new PACKET_DATA(strOut.c_str(), strOut.size(),nMode, wParam);
	bool ret = PostThreadMessage(m_nThreadID, WM_SEND_PACK, (WPARAM)pData, (LPARAM)hWnd);
	if (ret == false) {
		//发送失败也要删除堆对象
		delete pData;
	}

	return ret;
}
/*
bool CClientSocket::SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed)
{

	// 如果Socket未连接且工作线程未启动，创建新的工作线程（懒初始化）
	if (m_sock == INVALID_SOCKET && m_hThread == INVALID_HANDLE_VALUE) {
		m_hThread = (HANDLE)_beginthread(&CClientSocket::threadEntry, 0, this);
	}
	m_lock.lock();// 互斥锁(m_lock)保护共享资源
	// 将事件句柄与返回包列表关联 注意这里存的是&，所以后续操作m_mapAck的元素时会影响到lstPacks
	// 实际上，就在CClientSocket::threadFunc()中会将接受到的包插入m_mapAck->senond中
	auto pr = m_mapAck.insert(std::pair<HANDLE, std::list<CPacket>&>(pack.hEvent, lstPacks)); // insert返回的是<pair_iterator,bool>
	// 记录是否需要自动关闭
	m_mapAutoClosed.insert(std::pair<HANDLE, bool>(pack.hEvent, isAutoClosed));
	// 加入发送队列，工作线程(threadFunc)会不断从该队列取出数据发送
	m_lstSend.push_back(pack);
	m_lock.unlock();

	//同步等待响应​
	WaitForSingleObject(pack.hEvent, INFINITE);
	//查找并清理事件映射
	std::map<HANDLE, std::list<CPacket>&>::iterator it = m_mapAck.find(pack.hEvent);
	if (it != m_mapAck.end()) {
		m_lock.lock();
		m_mapAck.erase(it);
		m_lock.unlock();
		return true;
	}
	return false;
}
*/

bool CClientSocket::Send(const CPacket& pack)
{
	// 直接将包序列化为数据流 然后发送
	TRACE("m_sock = %d\r\n", m_sock);
	if (m_sock == -1)return false;
	std::string strOut;
	pack.Data(strOut);
	return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
}

void CClientSocket::SendPack(UINT nMSg, WPARAM wParam, LPARAM lParam)
{// TODO:定义一个消息的数据结构（数据和数据长度，模式）+回调消息的数据结构（HWND MESSAGE）
	PACKET_DATA data = *(PACKET_DATA*)wParam;
	delete (PACKET_DATA*)wParam;

	HWND hWnd = (HWND)lParam;

	if (InitSocket() == true) {
		
		int ret = send(m_sock, (char*)data.strData.c_str(), (int)data.strData.size(), 0);
		if (ret > 0) {
			size_t index = 0;
			std::string strBuffer;
			strBuffer.resize(BUFFER_SIZE);
			char* pBuffer = (char*)strBuffer.c_str();

			while (m_sock != INVALID_SOCKET) {
				int length = recv(m_sock, pBuffer, BUFFER_SIZE - index, 0);
				if (length > 0 || index >0){
					index += (size_t) length;
					size_t nLen = index;
					CPacket pack((BYTE*)pBuffer, nLen);
					if (nLen > 0) {
						// 为了实现跨线程的消息发送，这里把pack在堆上new了个对象，包装后发送，记得要在消息处理线程中，处理完成后delete掉
						::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), data.wParam); 
						if (data.nMode == CSM_AUTOCLOSE) {
							CloseSocket();
							return;
						}
					}
					index -= nLen;
					memmove(pBuffer, pBuffer + index, nLen);
				}
				else {//TODO:对方关闭了套接字 或者 网络设备异常
					CloseSocket();
					::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, 1);

				}
			}
		}
		else {
			CloseSocket();
			// 网络终止处理
			::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -1);
		}
	}
	else {
		::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -2);

	}

}

/*数据包发送与返回处理线程**********************************************************************************/
unsigned CClientSocket::threadEntry(void* arg)
{
	CClientSocket* thiz = (CClientSocket*)arg;
	thiz->threadFunc2();
	_endthreadex(0);
	return 0;
}
/*
void CClientSocket::threadFunc()
{
	// 缓冲区管理
	std::string strBuffer;
	strBuffer.resize(BUFFER_SIZE);
	char* pBuffer = (char*)strBuffer.c_str();
	int index = 0;

	InitSocket();
	// 当连接畅通且发送队列不为空时，不断从发送队列m_lstSend获取待发送数据包
	while (m_sock != INVALID_SOCKET) {
		if (m_lstSend.size() > 0) {
			TRACE("m_lstSend.size() = %d\r\n", m_lstSend.size());
			m_lock.lock();
			CPacket& head = m_lstSend.front();
			m_lock.unlock();
			// 调用Send()尝试发送，失败则跳过本次处理
			if (Send(head) == false) {
				TRACE("发送失败\r\n");
				continue;
			}
			// 响应接收处理
			auto it = m_mapAck.find(head.hEvent);
			if (it != m_mapAck.end()) {
				auto it0 = m_mapAutoClosed.find(head.hEvent);
				do {
					//非阻塞接收数据（累计到缓冲区）
					int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0);
					if ((length > 0) || (index > 0)) {
						index += length;
						size_t size = (size_t)index;
						// 构造CPacket对象处理接收数据
						CPacket pack((BYTE*)pBuffer, size);
						if (size > 0) {//TODO: 这里需要判断是否是完整的包。对于文件夹的获取，文件信息可能产生问题。
							pack.hEvent = head.hEvent;
							it->second.push_back(pack);
							memmove(pBuffer, pBuffer + size, index - size);
							index -= size;
							if (it0->second) {
								// 事件通知机制，通过SetEvent触发事件对象通知发送方（跨线程同步）
								SetEvent(head.hEvent);
								break;
							}
						}
					}
					// 连接异常处理
					else if (length <= 0 && index <= 0) {
						CloseSocket();
						SetEvent(head.hEvent);//等到服务器关闭命令之后，再通知事情完成
						if (it0 != m_mapAutoClosed.end()) {
							TRACE("SetEvent %d %d \r\n", head.sCmd, it0->second);
						}
						else {
							TRACE("异常情况 没有对应的pair\r\n");
						}
						break;
					}
				} while (it0->second == false);
			}
			m_lock.lock();
			m_lstSend.pop_front();//从发送队列中移除已经处理完成的请求包​​
			m_mapAutoClosed.erase(head.hEvent);
			m_lock.unlock();
			//断线重连
			if (InitSocket() == false) {
				InitSocket();
			}
		}
		Sleep(1);
	}
	CloseSocket();
}
*/


void CClientSocket::threadFunc2()
{
	SetEvent(m_eventInvoke); // 线程启动了 通知下外层的WaitForSingleObject
	MSG msg;
	while (::GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (m_mapFunc.find(msg.message) != m_mapFunc.end()) {
			/* m_mapFunc会包含的函数有：
			{WM_SEND_PACK,&CClientSocket::SendPack}
			*/
			(this->*m_mapFunc[msg.message])(msg.message, msg.wParam, msg.lParam);
		}
	}
}


