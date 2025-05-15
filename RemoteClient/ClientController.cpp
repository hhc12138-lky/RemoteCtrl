#include "pch.h"
#include "ClientController.h"

//静态成员变量需要独立初始化
std::map<UINT, CClientController::MSGFUNC> CClientController::m_mapFunc;
CClientController* CClientController::m_pInstance = NULL;

CClientController* CClientController::getInstance()
{
	// 懒汉式单例模式，首次调用时创建实例
	if (m_pInstance == nullptr)
	{
		m_pInstance = new CClientController();
		// 注册消息处理函数到 m_mapFunc，将消息类型与对应的成员函数绑定
		struct { UINT nMsg; MSGFUNC func; }MsgFuncs[] =
		{
			{WM_SEND_PACK, &CClientController::OnSendPacket},
			{WM_SEND_DATA, &CClientController::OnSendData},
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
	return nullptr;
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
	return info.result;
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

LRESULT CClientController::OnSendPacket(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return LRESULT();
}

LRESULT CClientController::OnSendData(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return LRESULT();
}

LRESULT CClientController::OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return m_statusDlg.ShowWindow(SW_SHOW);
}

LRESULT CClientController::OnShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam)
{

	return m_watchDlg.DoModal();
}
