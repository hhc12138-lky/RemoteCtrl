// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include "EdoyunTool.h"
#include "Command.h"
#include "EdoyunQueue.h"
#include <conio.h>



#ifdef _DEBUG
#define new DEBUG_NEW
#endif
//​​Visual C++ 特有的链接器指令​​，用于控制程序的 ​​入口点（Entry Point）​​ 和 ​​子系统（Subsystem）
//#pragma comment( linker, "/subsystem:windows /entry:WinMainCRTStartup" )
//#pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:mainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:WinMainCRTStartup" )

// 唯一的应用程序对象   

//#define INVOKE_PATH _T("C:\\Windows\\SysWOW64\\RemoteCtrl.exe")
#define INVOKE_PATH _T("C:\\Users\\edoyun\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl.exe")

CWinApp theApp;
using namespace std;


// 开机自启
bool ChooseAutoInvoke(const CString& strPath) {
	// 本机测试后记得删除相关内容
	// ！！！注意system(strCmd.c_str());执行后有个bug，给软链接设置到%SystemRoot\\SysWOW64下面了，暂时不知道为什么
	TCHAR wcsSystem[MAX_PATH] = _T(" ");
	if (PathFileExists(strPath)) {
		return true;
	}
	CString strInfo = _T("该程序只允许用于合法的用途!\n");
	strInfo += _T("继续运行该程序，将使得这台机器处于被监控状态!\n");
	strInfo += _T("如果你不希望这样，请按“取消”按钮，退出程序.\n");
	strInfo += _T("按下“是”按钮，该程序将被复制到你的机器上，并随系统启动而自动运行!\n");
	strInfo += _T("按下“否”按钮，程序只运行一次，不会在系统内留下任何东西!\n");
	int ret = MessageBox(NULL, strInfo, _T("警告"), MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);
	if (ret == IDYES) {
		// 方法1 写入注册表
		//WriteRegisterTable(strPath);

		// 方法2 写入启动目录
		if (!CEdoyunTool::WriteStartupDir(strPath)) {
			MessageBox(NULL, _T("复制文件失败，是否权限不足？\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
			return false;
		}
	}
	else if (ret == IDCANCEL) {
		return false;
	}
	return true;
}


/* 操作类型宏定义 */
#define IOCP_LIST_EMPTY 0    // 空操作
#define IOCP_LIST_PUSH  1    // 压入操作
#define IOCP_LIST_POP   2    // 弹出操作

/* 操作类型枚举 */
enum {
	IocpListEmpty,  // 对应 IOCP_LIST_EMPTY
	IocpListPush,   // 对应 IOCP_LIST_PUSH
	IocpListPop     // 对应 IOCP_LIST_POP
};

/* IOCP参数结构体，用于线程间通信*/
typedef struct IocpParam {
	int nOperator;         // 操作类型（使用宏或枚举值）
	std::string strData;   // 传输的数据内容
	_beginthread_proc_type cbFunc;// 回调

	// 带参构造函数
	IocpParam(int op, const char* sData, _beginthread_proc_type cb = NULL) {
		nOperator = op;
		strData = sData;
		cbFunc = cb;
	}

	// 无参构造函数
	IocpParam() {
		nOperator = -1;    // 默认无效操作标识
	}
} IOCP_PARAM;  // 类型别名


void threadmain(HANDLE hIOCP) {
	std::list<std::string> lstString; // 共享数据队列

	DWORD dwTransferred = 0;	// 接收I/O操作实际传输的字节数
	ULONG_PTR CompletionKey = 0; // 关联到IOCP的自定义参数，此处传递IOCP_PARAM结构体指针，类似于LPARAMA。
	OVERLAPPED* pOverlapped = NULL; //重叠I/O结构指针
	int count = 0, count0 = 0;
	// 阻塞等待指定的IOCP队列中的完成事件，直到有事件到达或线程被唤醒
	while (GetQueuedCompletionStatus(hIOCP, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE))
	{
		// dwTransferred == 0：通常表示线程退出信号（由PostQueuedCompletionStatus发送空包触发）
		if ((dwTransferred == 0) || (CompletionKey == NULL))
		{
			printf("thread is prepare to exit!\r\n");
			break;
		}

		// 将CompletionKey强制转换为IOCP_PARAM*，获取操作类型和数据。CompletionKey对应PostQueuedCompletionStatus的第三个参数
		/*这里可以强制转换是因为：ULONG_PTR是与指针等宽的无符号整数类型，所以可以将任何指针类型（如IOCP_PARAM*）存储到ULONG_PTR*/
		IOCP_PARAM* pParam = (IOCP_PARAM*)CompletionKey;
		// 压入操作
		if (pParam->nOperator == IocpListPush) {
			lstString.push_back(pParam->strData);
			count0++;
		}
		// 弹出操作
		else if (pParam->nOperator == IocpListPop) {
			std::string* pStr = NULL;
			if (lstString.size() > 0) {
				pStr = new std::string(lstString.front());
				lstString.pop_front();
			}
			// 若指定了回调函数cbFunc，则异步处理数据
			if (pParam->cbFunc) {
				pParam->cbFunc(pStr);
			}
			count++;
		}
		// 清空操作
		else if (pParam->nOperator == IocpListEmpty) {
			lstString.clear();
		}
		delete pParam;
	}
	printf("thread exit push:%d,pop:%d\r\n", count, count0);
}

// 工作线程函数
void threadQueueEntry(HANDLE hIOCP)
{
	threadmain(hIOCP);
	_endthread(); // 代码到此为止，会导致本地对象无法调用析构函数，从而导致内存泄漏
}


void func(void* arg)
{
	/*处理从队列中弹出的数据的示例回调函数*/

	// 将void*参数转型为string指针
	std::string* pstr = (std::string*)arg;
	if (pstr != NULL) {
		// 输出从列表中弹出的字符串内容
		printf("pop from list:%s\r\n", pstr->c_str());
		// 释放动态内存
		delete pstr;
	}
	else {
		printf("list is empty, no data!\r\n");
	}
} 

// 图1代码
void test()
{
	//性能：CEdoyunQueue push性能高 pop性能仅1/4
	//    list push性能比pop低
	CEdoyunQueue<std::string> lstStrings;
	ULONGLONG tick0 = GetTickCount64(), tick = GetTickCount64(), total = GetTickCount64();

	// 第一阶段：CEDoyunQueue push测试（1000ms窗口）
	while (GetTickCount64() - total <= 1000) {
		lstStrings.PushBack("hello world");
		tick0 = GetTickCount64();
	}
	size_t count = lstStrings.Size();
	printf("lstStrings done!size %d\r\n", count);

	// 图2代码（接续）
	// 第二阶段：CEDoyunQueue pop测试（1000ms窗口）
	total = GetTickCount64();
	while (GetTickCount64() - total <= 1000) {
		std::string str;
		lstStrings.PopFront(str);
		tick = GetTickCount64();
	}
	printf("lstStrings done!size %d\r\n", count - lstStrings.Size()); 
	lstStrings.Clear();

	// 第三阶段：std::list测试
	std::list<std::string> lstData;
	total = GetTickCount64();
	// push测试（1000ms窗口）
	while (GetTickCount64() - total <= 1000) {
		lstData.push_back("hello world");
	}
	count = lstData.size();
	printf("lstData push done!size %d\r\n", lstData.size());

	// pop测试（250ms窗口，标注压力测试编号2401040003）
	total = GetTickCount64();
	while (GetTickCount64() - total <= 250) {
		if (lstData.size() > 0) lstData.pop_front();
	}
	printf("lstData pop done!size %d\r\n", (count - lstData.size()) * 4);
}

// 图3注释（测试分层说明）
/*
1 bug测试/功能测试
2 关键因素的测试（内存泄漏、运行的稳定性、条件性）
3 压力测试（可靠性测试） 2401040003
4 性能测试
*/

int main()
{
	if (!CEdoyunTool::Init) return 1;

	//printf("press any key to exit ...\r\n");

	for (int i = 0;i < 10;i++) {
		test();
	}

	
	/*
	if (CEdoyunTool::IsAdmin) { 
		if (!CEdoyunTool::Init) return 1;
		if (!ChooseAutoInvoke(INVOKE_PATH)) {
			CCommand cmd;
			int ret = CServerSocket::getInstance()->Run(&CCommand::RunCommand, &cmd);
			switch (ret) {
			case -1:
				MessageBox(NULL, _T("网络初始化异常，未能成功初始hi，请检查网络状态！"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
				break;
			case -2:
				MessageBox(NULL, _T("多次无法正常接入用户，结束程序！"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
				break;
			}
		}
	}
	else {
		// 获取管理员权限、使用该权限创建进程
		if (CEdoyunTool::RunAsAdmin() == false) {
			CEdoyunTool::ShowError();
			return 1;
		}
	}
	return 0;
	*/
}




