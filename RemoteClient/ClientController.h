#pragma once
#include "ClientSocket.h"
#include "CWatchDialog.h"
#include "RemoteClientDlg.h"
#include "StatusDlg.h"
#include "Resource.h"

#include<map>

#define WM_SEND_PACK (WM_USER+1) // 发送包
#define WM_SEND_DATA (WM_USER+2) // 发送数据
#define WM_SHOW_STATUS (WM_USER+3) // 显示状态
#define WM_SHOW_WATCH (WM_USER+4) // 远程监控
#define WM_SEND_MESSAGE (WM_USER+0x1000)// 自定义消息处理


class CClientController
{
public:
	// 获取全局唯一对象
	static CClientController* getInstance();
	//初始化
	int InitController();
	//启动
	int Invoke(CWnd*& pMainWnd);
	//发送消息
	LRESULT SendMessage(MSG msg);

protected:
	CClientController():m_statusDlg(&m_remoteDlg),m_watchDlg(&m_remoteDlg) {
		m_hThread = INVALID_HANDLE_VALUE;
		m_hThreadID = -1;
	}
	~CClientController() {
		WaitForSingleObject(m_hThread, 100);

	}
	void threadFunc();
	static unsigned __stdcall threadEntry(void* arg);
	static void releaseInstance() {
		if (m_pInstance) {
			delete m_pInstance;
			m_pInstance = nullptr;
		}
	}

	//LRESULT 是一个长指针类型（LONG_PTR），通常用于表示消息处理的结果。
	//WPARAM 是一个无符号整数指针类型（UINT_PTR），通常用于传递消息或函数调用的第一个附加参数。
	//LPARAM 是一个长指针类型（LONG_PTR），通常用于传递消息或函数调用的附加参数（第二个）。
	//如果需要传递更复杂的数据，可以将数据封装到一个结构体中，并通过 LPARAM 传递该结构体的指针。
	LRESULT OnSendPacket(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnSendData(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam);


private:
	// 消息信息结构体
	typedef struct MsgInfo{
		MSG msg;
		LRESULT result;
		// 构造函数
		MsgInfo(MSG m) {
			result = 0;
			memcpy(&msg, &m, sizeof(MSG));
		}
		// 拷贝构造函数
		MsgInfo(const MsgInfo& m) {
			result = 0;
			memcpy(&msg, &m.msg, sizeof(MSG));
		}
		// =操作符重载
		MsgInfo& operator=(const MsgInfo& m) {
			if (this != &m) {
				result = 0;
				memcpy(&msg, &m.msg, sizeof(MSG));
			}
			return *this;
		}
	}MSGINFO;

	typedef LRESULT(CClientController::* MSGFUNC)(UINT, WPARAM, LPARAM); //函数指针类型
	static std::map<UINT, MSGFUNC> m_mapFunc;
	CWatchDialog m_watchDlg;
	CRemoteClientDlg m_remoteDlg;
	CStatusDlg m_statusDlg;

	HANDLE m_hThread;
	unsigned m_hThreadID;

	static CClientController* m_pInstance; //单例对象
	class CHelper {
	public:
		CHelper() {
			CClientController::getInstance();
		}
		~CHelper() {
			CClientController::releaseInstance();
		}
	};

};

