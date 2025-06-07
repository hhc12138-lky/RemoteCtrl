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
#include <MSWSock.h>
#include "EdoyunServer.h"



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

void iocp();

int main()
{
	if (!CEdoyunTool::Init()) return 1;

	iocp();
	
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


// COverlapped类是对Windows OVERLAPPED结构的扩展封装
class COverlapped {
public:
	OVERLAPPED m_overlapped;  // 基础重叠结构
	DWORD m_operator;         // 操作类型标识
	char m_buffer[4096];      // 数据缓冲区
	COverlapped() {
		m_operator = 0;
		memset(&m_overlapped, 0, sizeof(m_overlapped));
		memset(m_buffer, 0, sizeof(m_buffer));
	}
};

// iocp与socket绑定的示例
void iocp() 
{
	
	EdoyunServer server;
	server.startService();
	getchar();
	
}


