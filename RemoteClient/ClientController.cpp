#include "pch.h"
#include "ClientController.h"

//静态成员变量需要独立初始化
std::map<UINT, CClientController::MSGFUNC> CClientController::m_mapFunc;
CClientController* CClientController::m_pInstance = NULL;
CClientController::CHelper CClientController::m_helper; // 静态成员变量的初始化

CClientController* CClientController::getInstance()
{
	// 懒汉式单例模式，首次调用时创建实例
	if (m_pInstance == nullptr)
	{
		m_pInstance = new CClientController();
		// 注册消息处理函数到 m_mapFunc，将消息类型与对应的成员函数绑定
		struct { UINT nMsg; MSGFUNC func; }MsgFuncs[] =
		{
			{WM_SHOW_STATUS, &CClientController::OnShowStatus},
			{WM_SHOW_WATCH, &CClientController::OnShowWatcher},
			{(UINT) - 1,NULL}
		};
		for (auto& msgFunc : MsgFuncs)
		{
			if (msgFunc.nMsg == -1) break;
			m_mapFunc[msgFunc.nMsg] = msgFunc.func;
		}
	}
	return m_pInstance;
}

int CClientController::InitController()
{
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientController::threadEntry, this, 0, &m_hThreadID); // 创建线程
	m_statusDlg.Create(IDD_DLG_STATUS, &m_remoteDlg);
	return 0;
}

int CClientController::Invoke(CWnd*& pMainWnd)
{
	pMainWnd = &m_remoteDlg;
	return m_remoteDlg.DoModal();
}

LRESULT CClientController::SendMessage(MSG msg)
{
	// 创建事件，同时把消息转为自定义结构体，使用PostThreadMessage将事件机器消息存入线程消息队列，并且注册为WM_SEND_MESSAGE
	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEvent == NULL) {
		return -2;
	}
	MSGINFO info(msg);
	::PostThreadMessage(m_hThreadID, WM_SEND_MESSAGE, (WPARAM)&info, (LPARAM)hEvent);
	::WaitForSingleObject(hEvent, INFINITE); // 等待事件对象被释放
	CloseHandle(hEvent);
	return info.result;
}

int CClientController::SendCommandPacket(int nCmd, bool bAutoClose, BYTE* pData, size_t nLength, std::list<CPacket>* plstPacks)
{
	CClientSocket* pClient = CClientSocket::getInstance();
	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	//TODO：不应该直接发送，而是投入队列
	
	std::list<CPacket> lstPacks;//应答结果的包
	if (plstPacks == NULL) {
		plstPacks = &lstPacks;
	}
	CloseHandle(hEvent);//回收实践句柄，防止资源耗尽
	pClient->SendPacket(CPacket(nCmd, pData, nLength, hEvent),lstPacks, bAutoClose);
	if (lstPacks.size() > 0) {
		return plstPacks->front().sCmd;
	}
	return -1;
}

int CClientController::DownFile(CString strPath)
{
	CFileDialog dlg(FALSE, NULL,
		strPath, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
		NULL, &m_remoteDlg);
	if (dlg.DoModal() == IDOK) {
		m_strRemote = strPath;
		m_strLocal = dlg.GetPathName();

		CString strLocal = dlg.GetPathName();
		m_hThreadDownload = (HANDLE)_beginthread(&CClientController::threadDownloadEntry, 0, this);

		if (WaitForSingleObject(m_hThreadDownload, 0) != WAIT_TIMEOUT) {
			return -1;
		}
		m_remoteDlg.BeginWaitCursor();
		m_statusDlg.m_info.SetWindowText(_T("命令正在执行中！"));
		m_statusDlg.ShowWindow(SW_SHOW);
		m_statusDlg.CenterWindow(&m_remoteDlg);
		m_statusDlg.SetActiveWindow();
	}

	return 0;
}

void CClientController::StartWatchScreen()
{
	m_isClosed = false;
	//m_watchDlg.SetParent(&m_remoteDlg);
	m_hThreadWatch = (HANDLE)_beginthread(&CClientController::threadWatchScreen, 0, this);
	m_watchDlg.DoModal();
	m_isClosed = true;
	WaitForSingleObject(m_hThreadWatch, 500);
}

void CClientController::threadWatchScreen()
{
	Sleep(50);
	while (!m_isClosed) {
		if (m_watchDlg.isFull() == false) {//更新数据到缓存
			std::list<CPacket> lstPacks;
			int ret = SendCommandPacket(6,true,NULL,0,&lstPacks);
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

void CClientController::threadWatchScreen(void* arg)
{
	CClientController* thiz = (CClientController*)arg;
	thiz->threadDownloadFile();
	_endthread();
}

void CClientController::threadDownloadFile()
{
	FILE* pFile = fopen(m_strLocal, "wb+");
	if (pFile == NULL) {
		AfxMessageBox(_T("本地没有权限保存该文件，或者文件无法创建！！！"));
		m_statusDlg.ShowWindow(SW_HIDE);
		m_remoteDlg.EndWaitCursor();
		return;
	}
	CClientSocket* pClient = CClientSocket::getInstance();

	do {
		int ret = SendCommandPacket(4, false, (BYTE*)(LPCSTR)m_strRemote, m_strRemote.GetLength());
		long long nLength = *(long long*)pClient->GetPacket().strData.c_str();
		if (nLength == 0) {
			AfxMessageBox("文件长度为零或者无法读取文件！！！");
			break;
		}
		long long nCount = 0;
		while (nCount < nLength) {
			ret = pClient->DealCommand();
			if (ret < 0) {
				AfxMessageBox("传输失败！！");
				TRACE("传输失败：ret = %d\r\n", ret);
				break;
			}
			fwrite(pClient->GetPacket().strData.c_str(), 1, pClient->GetPacket().strData.size(), pFile);
			nCount += pClient->GetPacket().strData.size();
		}
	} while (false);
	fclose(pFile);
	pClient->CloseSocket();
	m_statusDlg.ShowWindow(SW_HIDE);
	m_remoteDlg.EndWaitCursor();
	m_remoteDlg.MessageBox(_T("下载完成！！"), _T("完成"));


}

void CClientController::threadDownloadEntry(void* arg)
{
	CClientController* thiz = (CClientController*)arg;
	thiz->threadWatchScreen();
	_endthread();
}

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
	CClientController* thiz = (CClientController*)arg;
	thiz->threadFunc();
	_endthreadex(0);
	return 0;
}



LRESULT CClientController::OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return m_statusDlg.ShowWindow(SW_SHOW);
}

LRESULT CClientController::OnShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam)
{

	return m_watchDlg.DoModal();
}
