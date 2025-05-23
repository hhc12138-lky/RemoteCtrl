
// RemoteClient.cpp: 定义应用程序的类行为。
//

#include "pch.h"
#include "framework.h"
#include "RemoteClient.h"
#include "ClientController.h"

//在调试模式下重定义 new 操作符为 DEBUG_NEW，用于内存泄漏检测
#ifdef _DEBUG
#define new DEBUG_NEW
#endif


//定义消息映射：将 ID_HELP 命令映射到 CWinApp::OnHelp 处理函数
BEGIN_MESSAGE_MAP(CRemoteClientApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CRemoteClientApp 构造函数
CRemoteClientApp::CRemoteClientApp()
{
	// 设置重启管理器支持标志，允许应用程序在意外关闭后恢复
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的 CRemoteClientApp 对象

CRemoteClientApp theApp;


// 应用程序初始化入口点
BOOL CRemoteClientApp::InitInstance()
{
	// 初始化Windows公共控件（如按钮、列表框等）
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	// 调用基类的初始化函数
	CWinApp::InitInstance();

	// 启用对包含ActiveX控件的支持
	AfxEnableControlContainer();

	// 创建shell管理器，用于处理shell相关的控件（如文件浏览器）
	CShellManager *pShellManager = new CShellManager;

	// 设置默认的视觉管理器，启用Windows原生主题
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	//设置注册表键路径，用于存储应用程序配置（如窗口位置、用户设置等）。默认是示例文本，实际应改为公司/应用名。
	SetRegistryKey(_T("应用程序向导生成的本地应用程序"));

	CClientController::getInstance()->InitController();
	INT_PTR nResponse = CClientController::getInstance()->Invoke(m_pMainWnd);
	//CRemoteClientDlg dlg;
	//m_pMainWnd = &dlg;
	//INT_PTR nResponse = dlg.DoModal();

	if (nResponse == IDOK)
	{
		// TODO: 处理用户点击“确定”按钮关闭对话框后的逻辑（如保存数据）。
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: 来关闭对话框的代码 处理用户点击“取消”按钮关闭对话框后的逻辑（如保存数据）。
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
		TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
	}

	// 清理shell管理器
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

	//在特定条件下清理控制栏资源
#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	//返回FALSE表示不进入主消息循环（适用于对话框应用），直接退出程序。若返回TRUE，则会启动消息泵（如文档视图架构）。
	return FALSE;
}

