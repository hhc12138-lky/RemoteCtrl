#pragma once
#include "ClientSocket.h"
#include "CWatchDialog.h"
#include "RemoteClientDlg.h"
#include "StatusDlg.h"
#include "Resource.h"
#include "EdoyunTool.h"

#include<map>

//#define WM_SEND_PACK (WM_USER+1) // 发送包
//#define WM_SEND_DATA (WM_USER+2) // 发送数据
#define WM_SHOW_STATUS (WM_USER+3) // 显示状态对话框消息
#define WM_SHOW_WATCH (WM_USER+4) // 显示远程监控对话框消息
#define WM_SEND_MESSAGE (WM_USER+0x1000) // 通用消息处理


class CClientController
{
public:
	// 获取全局唯一对象（单例对象指针）
	static CClientController* getInstance();
	// 初始化控制器
	int InitController();
	// 启动主界面
	int Invoke(CWnd*& pMainWnd);
	// 发送消息
	LRESULT SendMessage(MSG msg);

	// 更新服务器地址
	void UpdataAddress(int nIP, int nPort) {
		CClientSocket::getInstance()->UpdataAddress(nIP, nPort);
	}
	// 处理命令
	int DealCommand() {
		return CClientSocket::getInstance()->DealCommand();
	}
	// 关闭连接
	void CloseSocket() {
		CClientSocket::getInstance()->CloseSocket();
	}

	/*
	1 查看磁盘分区
	2 查看指定目录下的文件
	3 打开文件
	4 下载文件
	9 删除文件
	5 鼠标操作
	6 发送屏幕内容
	7 锁机
	8 解锁
	1981 测试连接
	返回值：是命令号，如果小于0则是错误
	*/
	
	// 发送各种控制命令到服务器
	int SendCommandPacket(int nCmd, bool bAutoClose = true, BYTE* pData = NULL, size_t nLength = 0, std::list<CPacket>* plstPacks=NULL);

	// 获取客户端网络层的数据包转为图像对象
	int GetImage(CImage& image) {
		CClientSocket* pClient = CClientSocket::getInstance();
		return CEdoyunTool::Bytes2Image(image, pClient->GetPacket().strData);
	}

	// 开始屏幕监控
	void StartWatchScreen();

	// 下载文件
	int DownFile(CString strPath);

protected:
	// 监控线程 远程屏幕监控功能，使用单独线程处理
	void threadWatchScreen();
	// 监控线程入口
	static void threadWatchScreenEntry(void* arg);

	// 下载线程 远程文件下载功能，使用单独线程处理
	void threadDownloadFile();
	// 下载线程入口
	static void threadDownloadEntry(void* arg);

	/* Windows API 线程机制的设计
	1. 线程入口函数的强制约定
		Windows API (_beginthreadex/CreateThread) 要求线程入口函数必须满足特定的函数签名：
		unsigned __stdcall ThreadProc(void* arg); // 严格的原型约定
		必须是静态函数或全局函数（不能是非静态成员函数）
		参数只能是 void*，返回值是 unsigned
		如果直接尝试使用成员函数作为线程入口：
		// 错误！成员函数隐含this指针，不符合线程API要求
		_beginthreadex(NULL, 0, threadWatchScreen, NULL, 0, NULL);

	2. 非静态成员函数的访问问题
		业务函数需要访问类成员：
		threadWatchScreen() 需要操作 CClientController 的成员变量（如 m_watchDlg）
		但静态入口函数无法直接访问非静态成员
		解决方案是通过参数传递this指针，传递路线如下：
		StartWatchScreen()-> threadWatchScreenEntry() -> threadWatchScreen()
		DownFile()-> threadDownloadEntry() -> threadDownloadFile()
	*/

	// 构造与析构
	CClientController():m_statusDlg(&m_remoteDlg),m_watchDlg(&m_remoteDlg) {
		m_isClosed = true;
		m_hThreadWatch = INVALID_HANDLE_VALUE;
		m_hThreadDownload = INVALID_HANDLE_VALUE;
		m_hThread = INVALID_HANDLE_VALUE;
		m_hThreadID = -1;
	}
	~CClientController() {
		WaitForSingleObject(m_hThread, 100);

	}

	//消息处理线程
	void threadFunc();
	//消息处理线程入口
	static unsigned __stdcall threadEntry(void* arg);

	// 单例对象销毁
	static void releaseInstance() {
		if (m_pInstance) {
			delete m_pInstance;
			m_pInstance = nullptr;
		}
	}

	// 自定义消息的处理函数
	LRESULT OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam);// 状态对话框消息处理
	LRESULT OnShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam);// 监控对话框消息处理
	/*
	LRESULT 是一个长指针类型（LONG_PTR），通常用于表示消息处理的结果。
	WPARAM 是一个无符号整数指针类型（UINT_PTR），通常用于传递消息或函数调用的第一个附加参数。
	LPARAM 是一个长指针类型（LONG_PTR），通常用于传递消息或函数调用的附加参数（第二个）。
	如果需要传递更复杂的数据，可以将数据封装到一个结构体中，并通过 LPARAM 传递该结构体的指针。
	*/

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

	// 用于消息处理的 “函数指针类型”
	typedef LRESULT(CClientController::* MSGFUNC)(UINT, WPARAM, LPARAM);
	static std::map<UINT, MSGFUNC> m_mapFunc;// 消息映射表
	CWatchDialog m_watchDlg;// 监控对话框
	CRemoteClientDlg m_remoteDlg;// 主对话框
	CStatusDlg m_statusDlg;// 状态对话框

	HANDLE m_hThread;// 消息线程句柄
	HANDLE m_hThreadDownload;// 下载线程句柄
	HANDLE m_hThreadWatch;// 监控线程句柄
	CString m_strRemote;//下载文件的远程路径
	CString m_strLocal;//下载文件的本地路径
	bool m_isClosed;//监视是否关闭
	unsigned m_hThreadID;// 消息线程ID

	static CClientController* m_pInstance; //单例对象

	// 使用RAII技术确保程序退出时自动释放单例实例
	/*
	问题：
		单例模式中，CClientController 的实例是通过 new 动态创建的
		如果程序员忘记手动调用 releaseInstance()，会导致​​内存泄漏​​
	解决方案：
		利用 C++ 的​​静态对象析构顺序​​：
		程序退出时，​​静态对象会按照与构造相反的顺序析构​​，
		m_helper在CClientController类内部，所以CHelper的析构会先于CClientController的析构，
		这样可以保证CClientController::releaseInstance();的执行，来使用单例m_pInstance
	*/
	class CHelper {
	public:
		CHelper() {
			// getInstance是懒加载模式，所以这里可以不需要
			//CClientController::getInstance();
		}
		~CHelper() {
			CClientController::releaseInstance();
		}
	};
	static CHelper m_helper; 
};

