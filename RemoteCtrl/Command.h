#pragma once
#include <map>
#include <list>
#include <direct.h>
#include <atlimage.h>
#include <stdio.h>
#include <io.h>
#include "resource.h"
#include "Packet.h"
#include "LockInfoDialog.h"
#include "EdoyunTool.h"
#pragma warning(disable:4966) // fopen sprintf strcpy strstr 

class CCommand
{
public:
	CCommand();
	~CCommand();
	// 方法执行 
	int ExcuteCommand(int nCmd, std::list<CPacket>& lstPackets, CPacket& inPacket);
	// 静态回调方法
	static void RunCommand(void* arg, int status, std::list<CPacket>& lstPackets, CPacket& inPacket) {
		CCommand* thiz = (CCommand*)arg;
		// 根据 status 判断是否执行命令或显示错误消息
		if (status > 0) {
			int ret = thiz->ExcuteCommand(status, lstPackets, inPacket);
			if (ret != 0) {
				TRACE("执行命令失败：%d ret=%d\r\n", status, ret);
			} 
		}
		else {
			MessageBox(NULL, _T("无法正常接入用户，自动重试"), _T("接入用户失败！"), MB_OK | MB_ICONERROR);
		}
	}
	/* Command调用路径：
	1. RemoteCtrl的main：CServerSocket::getInstance()->Run(&CCommand::RunCommand, &cmd);

	2. CServerSocket的Run：m_callback(arg, ret, lstPackets, m_packet); 
	m_callback就是RunCommand，arg就是cmd（CCommand类型），lstPackets是需要发送回的包，m_packet是接受到的包含命令的包

	3. CCommand::RunCommand：CCommand* thiz = (CCommand*)arg; thiz->ExcuteCommand(status, lstPackets, inPacket);
	传入的arg就是一个初始的CCommand对象，目的是用于调用成员函数ExcuteCommand

	4. CCommand::ExcuteCommand：(this->*it->second)(listPackets, inPacket);
	(this->*it->second)先执行 -> 再执行 ->* ，最终取得成员函数指针，再通过(listPackets, inPacket)作为参数调用
	*/

protected:
	// 定义了一个​​成员函数指针类型​​，名为 CMDFUNC，它指向 CCommand 类的成员函数，且该成员函数返回 int 类型、无参数。
	// list<CPacket>&用于存储需要返回的包，inPacket存储已经发送过来的包
	typedef int(CCommand::* CMDFUNC)(std::list<CPacket>&, CPacket& inPacket);

	//命令号到函数指针的映射
	std::map<int, CMDFUNC> m_mapFunction;
	CLockInfoDialog dlg;
	unsigned threadid;

protected:
	
	/* 
	1. _beginthreadex要求的线程函数：
		a 需要有static独立于类实例​，并且不会隐式包含this指针；
		b 需要__stdcall确保参数从右向左压栈，且由被调用方清理栈；
		c 需要unsigned返回值，用于线程退出码；
		d 需要void* arg允许传递任意上下文数据（如this指针）
		比如 static unsigned __stdcall threadLockDlg(void* arg);

	2. _beginthread要求的线程函数
		有a和d即可，返回值为void。
		比如 static void threadWatchScreenEntry(void* arg);

	*/
	// 静态线程函数，用于创建锁定屏幕的对话框 由LockMachine触发
	static unsigned __stdcall threadLockDlg(void* arg)
	{
		// 传入的arg指针实际就是CCommand指针 只是void*传递然后用CCommand*复原，用于调用CCommand的成员对象
		CCommand* thiz = (CCommand*)arg;
		thiz->threadLockDlgMain();
		_endthreadex(0);
		return 0;
	}
	void threadLockDlgMain() 
	{
		TRACE("%s(%d):%d\r\n", __FUNCTION__, __LINE__, GetCurrentThreadId());
		// 创建并显示对话框
		dlg.Create(IDD_DIALOG_INFO, NULL);
		dlg.ShowWindow(SW_SHOW);
		// 设置全屏窗口 用以遮蔽后台窗口
		CRect rect;
		rect.left = 0;
		rect.top = 0;
		rect.right = GetSystemMetrics(SM_CXFULLSCREEN);//w1
		rect.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
		rect.bottom = LONG(rect.bottom * 1.10);
		TRACE("right = %d bottom = %d\r\n", rect.right, rect.bottom);
		dlg.MoveWindow(rect);
		// 居中显示文本
		CWnd* pText = dlg.GetDlgItem(IDC_STATIC);
		if (pText) {
			CRect rtText;
			pText->GetWindowRect(rtText);
			int nWidth = rtText.Width();//w0
			int x = (rect.right - nWidth) / 2;
			int nHeight = rtText.Height();
			int y = (rect.bottom - nHeight) / 2;
			pText->MoveWindow(x, y, rtText.Width(), rtText.Height());
		}

		//窗口置顶
		dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);

		// 锁定系统
		ShowCursor(false); //限制鼠标功能
		::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE); //隐藏任务栏
		dlg.GetWindowRect(rect); //限制鼠标活动范围
		rect.left = 0;
		rect.top = 0;
		rect.right = 1;
		rect.bottom = 1;
		ClipCursor(rect);

		// 消息循环，等待A键解锁
		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_KEYDOWN) {
				TRACE("msg:%08X wparam:%08x lparam:%08X\r\n", msg.message, msg.wParam, msg.lParam);
				if (msg.wParam == 0x41) {//按下a键 退出  ESC（1B)
					break;
				}
			}
		}

		// 解锁系统
		ClipCursor(NULL);// 释放鼠标限制
		ShowCursor(true);// 显示鼠标
		::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_SHOW);// 显示任务栏
		dlg.DestroyWindow();// 销毁对话框
	}

	// 获取驱动器信息
	int MakeDriverInfo(std::list<CPacket>& listPackets, CPacket& inPacket) {//1==>A 2==>B 3==>C ... 26==>Z
		std::string result;
		for (int i = 1; i <= 26; i++) { // 遍历A-Z驱动器
			if (_chdrive(i) == 0) {  // 尝试切换驱动器
				if (result.size() > 0)
					result += ',';
				result += 'A' + i - 1;  // 添加驱动器字母
			}
		}
		listPackets.push_back(CPacket(1, (BYTE*)result.c_str(), result.size()));
		return 0;
	}

	// 获取目录信息
	int MakeDirectoryInfo(std::list<CPacket>& listPackets, CPacket& inPacket) {
		std::string strPath = inPacket.strData;
		// 尝试切换目录
		if (_chdir(strPath.c_str()) != 0) {
			// 失败时发送空响应
			FILEINFO finfo;
			finfo.HasNext = FALSE; 
			listPackets.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
			OutputDebugString(_T("没有权限访问目录！！"));
			return -2;
		}
		// 查找目录中的文件和子目录
		_finddata_t fdata;
		intptr_t hfind = _findfirst("*", &fdata);
		if (hfind == -1) {
			// 空目录处理
			OutputDebugString(_T("没有找到任何文件！！"));
			FILEINFO finfo;
			finfo.HasNext = FALSE;
			listPackets.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
			return -3;
		}
		// 遍历目录内容
		int count = 0;
		do {
			FILEINFO finfo;
			finfo.IsDirectory = (fdata.attrib & _A_SUBDIR) != 0;
			memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
			//TRACE("%s\r\n", finfo.szFileName);
			listPackets.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
			count++;
		} while (!_findnext(hfind, &fdata));
		TRACE("MakeDirectoryInfo server total count = %d\r\n", count);
		// 发送结束信息到控制端
		FILEINFO finfo;
		finfo.HasNext = FALSE;
		listPackets.push_back(CPacket(2, (BYTE*)&finfo, sizeof(finfo)));
		return 0;
	}

	// 运行文件
	int RunFile(std::list<CPacket>& listPackets, CPacket& inPacket) {
		std::string strPath = inPacket.strData; // 提取文件路径
		ShellExecuteA(NULL, NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL); // 文件执行
		listPackets.push_back(CPacket(3, NULL, 0)); // 发送成功响应
		return 0;
	}

	// 将指定文件内容分块打包传输
	int DownloadFile(std::list<CPacket>& listPackets, CPacket& inPacket) {
		// 从输入数据包中获取文件路径
		std::string strPath = inPacket.strData;
		long long data = 0; // 初始化文件大小为0
		FILE* pFile = NULL; // 文件指针初始化为NULL
		errno_t err = fopen_s(&pFile, strPath.c_str(), "rb");// 安全方式打开文件（二进制只读模式），打开成功则返回0
		if (err != 0) {
			// 如果打开文件失败，推送一个包含0值(8字节)的数据包(命令号4)表示文件大小为0
			listPackets.push_back(CPacket(4, (BYTE*)&data, 8));
			return -1;
		}

		// 检查文件指针是否有效（冗余检查，因为fopen_s失败时pFile已经是NULL）
		if (pFile != NULL) {
			// 获取文件大小
			fseek(pFile, 0, SEEK_END); // 将文件指针移动到文件末尾
			data = _ftelli64(pFile); // 获取当前文件指针位置（即文件末尾 == 文件大小），_ftelli64支持大于2GB的文件
			listPackets.push_back(CPacket(4, (BYTE*)&data, 8)); // 推送文件大小信息(命令号4，8字节)，文件传输第一个包一定是文件大小信息，不含文件内容

			// 将文件指针重置回文件开头
			fseek(pFile, 0, SEEK_SET); 
			char buffer[1024] = ""; // 定义1KB的缓冲区
			size_t rlen = 0; // 初始化读取长度为0

			// 循环读取文件内容，每次读取最多1024字节， 将读取的数据推送为数据包(命令号4)
			do {
				rlen = fread(buffer, 1, 1024, pFile);
				listPackets.push_back(CPacket(4, (BYTE*)buffer, rlen));
			} while (rlen >= 1024);
			fclose(pFile);// 关闭文件
		}
		else {
			// 如果文件指针意外为NULL（理论上不会执行到这里）推送一个空数据包(命令号4)
			listPackets.push_back(CPacket(4, NULL, 0));
		}
		return 0;
	}

	// 删除文件
	int DeleteLocalFile(std::list<CPacket>& listPackets, CPacket& inPacket)
	{
		std::string strPath = inPacket.strData;// 路径获取

		// 路径编码转换
		TCHAR sPath[MAX_PATH] = _T("");
		//mbstowcs(sPath, strPath.c_str(), strPath.size()); //中文容易乱码 使用下面的方法
		// 多字节路径转换为宽字符路径（处理中文路径）
		MultiByteToWideChar(
			CP_ACP, 0, strPath.c_str(), strPath.size(), sPath,
			sizeof(sPath) / sizeof(TCHAR)); 

		DeleteFileA(strPath.c_str()); //删除
		listPackets.push_back(CPacket(9, NULL, 0)); //无论删除是否成功，都返回成功响应(简化设计)
		return 0;
	}

	// 鼠标事件
	int MouseEvent(std::list<CPacket>& listPackets, CPacket& inPacket)
	{
		MOUSEEV mouse;
		memcpy(&mouse, inPacket.strData.c_str(), sizeof(MOUSEEV));
		DWORD nFlags = 0;
		switch (mouse.nButton) {
		case 0://左键
			nFlags = 1;
			break;
		case 1://右键
			nFlags = 2;
			break;
		case 2://中键
			nFlags = 4;
			break;
		case 4://没有按键
			nFlags = 8;
			break;
		}

		if (nFlags != 8)SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
		switch (mouse.nAction)
		{
		case 0://单击
			nFlags |= 0x10;
			break;
		case 1://双击
			nFlags |= 0x20;
			break;
		case 2://按下
			nFlags |= 0x40;
			break;
		case 3://放开
			nFlags |= 0x80;
			break;
		default:
			break;
		}

		TRACE("mouse event : %08X x %d y %d\r\n", nFlags, mouse.ptXY.x, mouse.ptXY.y);
		switch (nFlags)
		{
		case 0x21://左键双击
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
		case 0x11://左键单击
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x41://左键按下
			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
			TRACE("mouse event: 左键按下\r\n");
			break;
		case 0x81://左键放开
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x22://右键双击
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
		case 0x12://右键单击
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x42://右键按下
			mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x82://右键放开
			mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x24://中键双击
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
		case 0x14://中键单击
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x44://中键按下
			mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x84://中键放开
			mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
			break;
		case 0x08://单纯的鼠标移动
			mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
			TRACE("mouse event: 单纯的鼠标移动\r\n");
			break;
		}
		listPackets.push_back(CPacket(5, NULL, 0));
		return 0;
	}

	// 发送屏幕截图​
	int SendScreen(std::list<CPacket>& listPackets, CPacket& inPacket)
	{
		CImage screen;//GDI
		HDC hScreen = ::GetDC(NULL);// 获取整个屏幕的设备上下文
		int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);// 获取色彩深度(24/32bit)   ARGB8888 32bit RGB888 24bit RGB565  RGB444
		int nWidth = GetDeviceCaps(hScreen, HORZRES);// 水平分辨率
		int nHeight = GetDeviceCaps(hScreen, VERTRES);// 垂直分辨率
		screen.Create(nWidth, nHeight, nBitPerPixel);// 创建兼容位图
		BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);// BitBlt 执行位块传输完成屏幕拷贝
		ReleaseDC(NULL, hScreen);// 立即释放屏幕DC
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0);// 创建可移动内存块
		if (hMem == NULL)return -1;
		IStream* pStream = NULL;
		HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);// 绑定内存到流
		if (ret == S_OK) {
			screen.Save(pStream, Gdiplus::ImageFormatPNG);// PNG编码
			LARGE_INTEGER bg = { 0 };// 重置流指针
			pStream->Seek(bg, STREAM_SEEK_SET, NULL);
			PBYTE pData = (PBYTE)GlobalLock(hMem);// 锁定内存
			SIZE_T nSize = GlobalSize(hMem);// 获取数据大小
			listPackets.push_back(CPacket(6, (BYTE*)pData, nSize));// 发送数据包
			GlobalUnlock(hMem);// 必须解锁后才能释放
		}
		
		pStream->Release();// COM对象引用计数减1
		GlobalFree(hMem);// 释放全局内存
		screen.ReleaseDC();// 释放图像DC
		return 0;
	}

	// 锁机
	int LockMachine(std::list<CPacket>& listPackets, CPacket& inPacket)
	{
		// 检查对话框窗口是否已存在
		if ((dlg.m_hWnd == NULL) || (dlg.m_hWnd == INVALID_HANDLE_VALUE)) {
			//_beginthreadex创建独立UI线程
			_beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid);
			TRACE("threadid=%d\r\n", threadid);
		}
		// 无论锁定是否成功都返回成功(命令号7)
		listPackets.push_back(CPacket(7, NULL, 0));
		return 0;
	}

	// 解锁
	int UnlockMachine(std::list<CPacket>& listPackets, CPacket& inPacket)
	{
		// 原始方案（直接窗口消息）
		//dlg.SendMessage(WM_KEYDOWN, 0x41, 0x01E0001);
		//::SendMessage(dlg.m_hWnd, WM_KEYDOWN, 0x41, 0x01E0001);

		// 发送虚拟按键消息(0x41对应'A'键)，PostThreadMessage异步执行实现线程间通信
		PostThreadMessage(threadid, WM_KEYDOWN, 0x41, 0);
		// 统一响应​​：返回空数据包(命令号8)
		listPackets.push_back(CPacket(8, NULL, 0));
		return 0;
	}

	// 测试
	int TestConnect(std::list<CPacket>& listPackets, CPacket& inPacket)
	{	
		listPackets.push_back(CPacket(1981, NULL, 0));
		return 0;
	}
};

