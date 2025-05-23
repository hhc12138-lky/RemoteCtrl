
// RemoteClient.h: PROJECT_NAME 应用程序的主头文件
//

#pragma once

// 检查是否已经包含了 __AFXWIN_H__（MFC核心头文件）；如果没有包含，则报错提示用户必须先包含 pch.h（预编译头文件）
#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'pch.h' 以生成 PCH"
#endif


//包含资源文件，该文件定义了应用程序使用的资源ID（如对话框、图标等）
#include "resource.h"	


class CRemoteClientApp : public CWinApp
{
public:
	CRemoteClientApp();

// 继承并重写重写InitInstance
public:
	// CRemoteClientApp继承自MFC的 CWinApp 基类，虚函数 InitInstance()就是MFC应用程序的初始化入口点
	virtual BOOL InitInstance();

	//声明消息映射宏，用于处理Windows消息
	DECLARE_MESSAGE_MAP()
};

//声明全局的应用程序对象 theApp
extern CRemoteClientApp theApp;
