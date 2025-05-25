#include "pch.h"
#include "ClientController.h"

// 初始化静态成员变量：消息处理映射表、单例实例指针和RAII辅助对象
std::map<UINT, CClientController::MSGFUNC> CClientController::m_mapFunc;
CClientController* CClientController::m_pInstance = NULL;
CClientController::CHelper CClientController::m_helper; 

CClientController* CClientController::getInstance()
{
	// 懒汉式单例模式，首次调用时创建实例
	if (m_pInstance == nullptr)
	{
		m_pInstance = new CClientController();
		//定义结构体数组，存储消息类型和对应的处理函数
		struct { UINT nMsg; MSGFUNC func; }MsgFuncs[] =
		{
			{WM_SHOW_STATUS, &CClientController::OnShowStatus},
			{WM_SHOW_WATCH, &CClientController::OnShowWatcher},
			{(UINT) - 1,NULL}//消息处理数组的​​结束标记​​，用于标识数组遍历的终止位置。
		};
		// 注册消息处理函数到 m_mapFunc，将消息类型与对应的成员函数绑定
		for (auto& msgFunc : MsgFuncs)
		{
			if (msgFunc.nMsg == -1) break;
			m_mapFunc[msgFunc.nMsg] = msgFunc.func;
		}
	}
	return m_pInstance;
}

// 控制器初始化​
int CClientController::InitController()
{
	// 创建主工作线程，线程启动时_beginthreadex会自动填充m_hThreadID
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientController::threadEntry, this, 0, &m_hThreadID); // 创建线程
	//初始化状态对话框。提前创建状态对话框（但不显示），目的是​​避免首次使用时延迟​​。将m_remoteDlg设为其父窗口，确保对话框层级关系和模态行为正确。
	m_statusDlg.Create(IDD_DLG_STATUS, &m_remoteDlg);
	return 0;
}

// 启动主界面​
int CClientController::Invoke(CWnd*& pMainWnd)
{
	//设置主窗口指针
	pMainWnd = &m_remoteDlg;
	//以模态方式显示主对话框
	return m_remoteDlg.DoModal();
}

// 线程间消息发送​
LRESULT CClientController::SendMessage(MSG msg)
{
	// 创建事件，通过事件对象实现跨线程同步通信
	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEvent == NULL) {
		return -2;
	}
	// 同时把消息转为自定义结构体，使用PostThreadMessage将事件机器消息存入线程消息队列，并且注册为WM_SEND_MESSAGE
	MSGINFO info(msg);
	::PostThreadMessage(m_hThreadID, WM_SEND_MESSAGE, (WPARAM)&info, (LPARAM)hEvent);//这里的m_hThreadID已经在InitController的_beginthreadex中初始化过了，有固定的线程id值
	::WaitForSingleObject(hEvent, INFINITE); // 等待事件对象被释放
	CloseHandle(hEvent);
	return info.result;
}

// 命令发送​
bool CClientController::SendCommandPacket(HWND hWnd, int nCmd, bool bAutoClose, BYTE* pData, size_t nLength, WPARAM wParam)
{
	/*
	int nCmd,           // 命令类型编号（如1=查看磁盘分区，6=获取屏幕内容等）
	bool bAutoClose,    // 是否自动关闭连接
	BYTE* pData,        // 附加数据指针
	size_t nLength,     // 附加数据长度
	std::list<CPacket>* plstPacks // 返回数据包列表
	*/
	//获取全局唯一的Socket通信实例
	CClientSocket* pClient = CClientSocket::getInstance();
	
	// 发送CPacket包含请求，返回lstPacks
	return pClient->SendPacket(hWnd, CPacket(nCmd, pData, nLength), bAutoClose, wParam);
}

/*视频监控线程********************************************************************************************************/
void CClientController::threadWatchScreen()
{
	Sleep(50);
	while (!m_isClosed) {
		//当监视未关闭时，更新数据到缓存
		if (m_watchDlg.isFull() == false) {
			// 发送屏幕请求命令，lstPacks存储返回数据包列表
			std::list<CPacket> lstPacks;
			int ret = SendCommandPacket(m_watchDlg.GetSafeHwnd(),6, true, NULL, 0);
			//TODO 添加消息响应函数 WM_SEND_PACK_ACK
			// //TODO 控制发送频率
			// 处理服务端返回
			if (ret == 6) {
				if (CEdoyunTool::Bytes2Image(m_watchDlg.GetImage(), lstPacks.front().strData) == 0) {
					m_watchDlg.SetImageStatus(true);
				}
				else {
					TRACE("获取数据失败! %d\r\n",ret);
				}
			}
			else {
				TRACE("获取数据失败!\r\n");
			}
		}
		else Sleep(1);
	}
}


void CClientController::threadWatchScreenEntry(void* arg)
{
	CClientController* thiz = (CClientController*)arg;
	thiz->threadWatchScreen();
	_endthread();
}

void CClientController::StartWatchScreen()
{
	m_isClosed = false; // 将监控标志位设为false，表示监控开始
	//m_watchDlg.SetParent(&m_remoteDlg);
	m_hThreadWatch = (HANDLE)_beginthread(&CClientController::threadWatchScreenEntry, 0, this);
	m_watchDlg.DoModal();// 以模态方式显示监控窗口，阻塞当前线程直到对话框关闭
	m_isClosed = true;// 设置关闭标志通知监控线程退出
	WaitForSingleObject(m_hThreadWatch, 500); // 等待最多500ms让线程安全退出
}

void CClientController::DownloadEnd()
{
	m_statusDlg.ShowWindow(SW_HIDE);
	m_remoteDlg.EndWaitCursor();
	m_remoteDlg.MessageBox(_T("下载完成！！"), _T("完成"));
}

/*文件下载线程********************************************************************************************************/
void CClientController::threadDownloadFile()
{
	// 打开本地文件并处理失败情况
	FILE* pFile = fopen(m_strLocal, "wb+");
	if (pFile == NULL) {
		AfxMessageBox(_T("本地没有权限保存该文件，或者文件无法创建！！！"));
		m_statusDlg.ShowWindow(SW_HIDE);
		m_remoteDlg.EndWaitCursor();
		return;
	}

	// 文件下载
	CClientSocket* pClient = CClientSocket::getInstance();
	do {
		// 发送下载命令
		int ret = SendCommandPacket(m_remoteDlg, 4, false, (BYTE*)(LPCSTR)m_strRemote, m_strRemote.GetLength(), (WPARAM)pFile);
		// 获取文件总长度(服务器返回的第一个数据包中包含文件大小)
		long long nLength = *(long long*)pClient->GetPacket().strData.c_str();
		if (nLength == 0) {
			AfxMessageBox("文件长度为零或者无法读取文件！！！");
			break;
		}
		//文件数据循环接收
		long long nCount = 0;// 已接收字节计数器
		while (nCount < nLength) {
			// 处理服务器命令(接收数据)
			ret = pClient->DealCommand();
			if (ret < 0) {
				AfxMessageBox("传输失败！！");
				TRACE("传输失败：ret = %d\r\n", ret);
				break;
			}
			// 写入接收到的数据到文件
			fwrite(pClient->GetPacket().strData.c_str(), 1, pClient->GetPacket().strData.size(), pFile);
			nCount += pClient->GetPacket().strData.size();
		}
	} while (false);
	// 清理资源
	fclose(pFile);
	pClient->CloseSocket();
	// 更新UI状态
	m_statusDlg.ShowWindow(SW_HIDE);
	m_remoteDlg.EndWaitCursor();
	m_remoteDlg.MessageBox(_T("下载完成！！"), _T("完成"));
}

void CClientController::threadDownloadEntry(void* arg)
{
	CClientController* thiz = (CClientController*)arg;
	thiz->threadDownloadFile();
	_endthread();
}

int CClientController::DownFile(CString strPath)
{
	//创建"另存为"对话框
	CFileDialog dlg(FALSE, NULL, strPath, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, NULL, &m_remoteDlg);
	//用户点击"保存"后执行下载准备
	if (dlg.DoModal() == IDOK) {
		m_strRemote = strPath; // 获取用户选择的远程文件路径
		m_strLocal = dlg.GetPathName(); // 获取用户选择的本地保存路径

		FILE* pFile = fopen(m_strLocal, "wb+");
		if (pFile == NULL) {
			AfxMessageBox(_T("本地没有权限保存该文件，或者文件无法创建！！！"));
			return -1;
		}
		int ret = SendCommandPacket(m_remoteDlg, 4, false, (BYTE*)(LPCSTR)m_strRemote, m_strRemote.GetLength(), (WPARAM)pFile);
		// 创建下载线程 指定threadDownloadEntry为中转函数 传递this指针以便访问成员变量
		//m_hThreadDownload = (HANDLE)_beginthread(&CClientController::threadDownloadEntry, 0, this);

		// 立即检查下载线程是否已经意外终止
		//if (WaitForSingleObject(m_hThreadDownload, 0) != WAIT_TIMEOUT) {
		//	return -1;
		//}

		// UI状态更新​
		m_remoteDlg.BeginWaitCursor();  // 显示等待光标
		m_statusDlg.m_info.SetWindowText(_T("命令正在执行中！"));  // 设置状态文本
		m_statusDlg.ShowWindow(SW_SHOW);  // 显示状态对话框
		m_statusDlg.CenterWindow(&m_remoteDlg);  // 居中显示
		m_statusDlg.SetActiveWindow();  // 设为活动窗口
	}

	return 0;
}

/*消息处理线程********************************************************************************************************/
void CClientController::threadFunc()
{
	MSG msg;
	// 使用 GetMessage 函数从线程的消息队列中获取消息（即SendMessage中PostThreadMessage存入的）
	// GetMessage将消息内容存入msg.wParam，事件存入msg.lParam，消息类型存入msg.message
	while (::GetMessage(&msg, NULL, 0, 0))
	{
		// TranslateMessage 函数，将虚拟键消息（如按键消息）转换为字符消息（如字符输入消息）
		// 调用 DispatchMessage 函数，将消息分发到对应的窗口过程
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);

		// 是否为自定义消息
		if (msg.message == WM_SEND_MESSAGE) {
			MSGINFO* pmsg = (MSGINFO*)msg.wParam;
			HANDLE hEvent = (HANDLE)msg.lParam;
			auto it = m_mapFunc.find(msg.message);
			if (it != m_mapFunc.end())
			{
				// 如果找到 就执行 然后从消息队列中删除消息
				pmsg->result = (this->*(it->second))(pmsg->msg.message, pmsg->msg.wParam, pmsg->msg.lParam);
			}
			else {
				pmsg->result = -1;
			}
			SetEvent(hEvent); // 释放事件对象
		}
		else {
			// 处理其他类型的消息
			std::map<UINT, MSGFUNC>::iterator it = m_mapFunc.find(msg.message);
			if (it != m_mapFunc.end())
			{
				(this->*(it->second))(msg.message, msg.wParam, msg.lParam);
			}
		}
		
	}
}

unsigned __stdcall CClientController::threadEntry(void* arg)
{
	// threadEntry在CClientController::InitController()启动
	CClientController* thiz = (CClientController*)arg;
	thiz->threadFunc();
	_endthreadex(0);
	return 0;
}

/*消息处理函数********************************************************************************************************/

LRESULT CClientController::OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	// 非模态展示状态对话框
	return m_statusDlg.ShowWindow(SW_SHOW);
}

LRESULT CClientController::OnShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	// 以阻塞方式显示监控对话框
	return m_watchDlg.DoModal();
}
