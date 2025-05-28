// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include "ServerSocket.h"
#include "EdoyunTool.h"
#include "Command.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
//​​Visual C++ 特有的链接器指令​​，用于控制程序的 ​​入口点（Entry Point）​​ 和 ​​子系统（Subsystem）
//#pragma comment( linker, "/subsystem:windows /entry:WinMainCRTStartup" )
//#pragma comment( linker, "/subsystem:windows /entry:mainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:mainCRTStartup" )
//#pragma comment( linker, "/subsystem:console /entry:WinMainCRTStartup" )

// 唯一的应用程序对象   

CWinApp theApp;
using namespace std;


/*
 开机启动时，程序执行的权限是跟随启动用户的权限的；如果两者不一致，就会启动失败。（属性-链接器-清单文件-UAC执行级别-asInvoker）
 开机启动对环境变量有影响，如果依赖dll，则可能启动失败：
	1.可以编译时设置为 依据MFC静态库的方式编译（属性-高级-高级属性-MFC的使用-在静态库中使用MFC）
	2. 或者复制对应dll文件到system32或者SysWOW64下面
 反常识的：system32多是64位程序，而SysWOW64下面多是32位程序
*/
void WriteRegisterTable(const CString& strPath) {
	CString strSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"); // 注册表路径，自己到目标电脑上找

	char sPath[MAX_PATH] = "";
	char sSys[MAX_PATH] = "";
	std::string strExe = "\\RemoteCtrl.exe ";
	GetCurrentDirectoryA(MAX_PATH, sPath);
	GetSystemDirectoryA(sSys, MAX_PATH);
	std::string strCmd = "mklink " + std::string(sSys) + strExe + std::string(sPath) + strExe;
	// 执行系统程序 设置软链接（注意可能有个bug，可能会创建到%SystemRoot\\SysWOW64而非%SystemRoot\\system32）
	int ret = system(strCmd.c_str());
	TRACE("执行操作%s 返回值 %d\n", strCmd.c_str(), ret);

	//从根键HKEY_LOCAL_MACHINE打开子键strSubKey
	HKEY hKey = NULL;
	int ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSubKey, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey);
	// 打开子键strSubKey失败
	if (ret != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		MessageBox(NULL, _T("设置自动开机启动失败！是否权限不足？\r\n程序启动失败！"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
		::exit(0);
	}
	// 打开子键strSubKey成功 则设置注册表项
	ret = RegSetValueEx(hKey, _T("RemoteCtrl"), 0, REG_EXPAND_SZ, (BYTE*)(LPCTSTR)strPath, strPath.GetLength() * sizeof(TCHAR));
	if (ret != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		MessageBox(NULL, _T("设置自动开机启动失败！是否权限不足？\r\n程序启动失败！"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
		::exit(0);
	}
	RegCloseKey(hKey);
}


void WriteStartupDir(const CString& strPath) {
	CString strCmd = GetCommandLine();
	strCmd.Replace(_T("\""), _T(""));
	BOOL ret = CopyFile(strCmd, strPath, FALSE);
	//fopen CFile system(copy) CopyFile OpenFile
	if (ret == FALSE) {
		MessageBox(NULL, _T("复制文件失败，是否权限不足?\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
		::exit(0);
	}
}

void ChooseAutoInvoke() {
	// 本机测试后记得删除相关内容

	//CString strPath = CString(_T("%SystemRoot\\system32\\RemoteCtrl.exe"));
	// ！！！注意system(strCmd.c_str());执行后有个bug，给软链接设置到%SystemRoot\\SysWOW64下面了，暂时不知道为什么
	TCHAR wcsSystem[MAX_PATH] = _T(" ");
	//CString strPath = CString(_T("C:\\Windows\\SysWOW64\\RemoteCtrl.exe")); // 方法1的路径
	CString strPath = _T("C:\\Users\\edoyun\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\RemoteCtrl.exe"); // 方法2的路径

	if (PathFileExists(strPath)) {
		return;
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
		WriteStartupDir(strPath);
	}
	else if (ret == IDCANCEL) {
		::exit(0);
	}
}

// 使用回调函数 将函数作为参数传递 实现解耦合
int main()
{
	int nRetCode = 0;  
	 
	HMODULE hModule = ::GetModuleHandle(nullptr);

	if (hModule != nullptr)
	{
		// 初始化 MFC 并在失败时显示错误    
		if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
		{
			// TODO: 在此处为应用程序的行为编写代码。
			wprintf(L"错误: MFC 初始化失败\n");
			nRetCode = 1;
		}
		else
		{
			CCommand cmd;
			ChooseAutoInvoke();
			int ret = CServerSocket::getInstance()->Run(&CCommand::RunCommand, &cmd);
			switch (ret) {
			case -1:
				MessageBox(NULL, _T("网络初始化异常，未能成功初始hi，请检查网络状态！"), _T("网络初始化失败"), MB_OK | MB_ICONERROR);
				::exit(0);
				break;
			case -2:
				MessageBox(NULL, _T("多次无法正常接入用户，结束程序！"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
				::exit(0);
				break;

			}
		}
	}
	else
	{
		// TODO: 更改错误代码以符合需要
		wprintf(L"错误: GetModuleHandle 失败\n");
		nRetCode = 1;
	}

	return nRetCode;
}




