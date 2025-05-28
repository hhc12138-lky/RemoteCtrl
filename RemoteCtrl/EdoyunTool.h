#pragma once
class CEdoyunTool
{
public:
	static void Dump(BYTE* pData, size_t nSize)
	{
		std::string strOut;
		for (size_t i = 0; i < nSize; i++)
		{
			char buf[8] = "";
			if (i > 0 && (i % 16 == 0))strOut += "\n";
			snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
			strOut += buf;
		}
		strOut += "\n";
		OutputDebugStringA(strOut.c_str());
	}

	static bool IsAdmin()
	{
		HANDLE hToken = NULL;

		// 获取当前进程的令牌句柄
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
		{
			ShowError();
			return false;
		}
		TOKEN_ELEVATION eve;
		DWORD len = 0;
		// 根据当前进程的令牌句柄查询提升信息。TokenElevation专用于检测 ​​UAC（用户账户控制）提升状态​​，判断进程是否以管理员权限运行
		if (GetTokenInformation(hToken, TokenElevation, &eve, sizeof(eve), &len) == FALSE)
		{
			ShowError();
			return false;
		}
		CloseHandle(hToken);  // 关闭令牌句柄

		// 根据返回数据长度判断管理员权限
		if (len == sizeof(eve))
		{
			return eve.TokenIsElevated;
		}
		printf("length of tokeninformation is %d\r\n", len);
		return false;
	}

	static bool RunAsAdmin()
	{
		//获取管理员权限、使用该权限创建进程

		// 注意 需要服务端首先是win10 专业版 然后需要在 本地安全策略中 “开启Administrator账户”&&“禁止空密码只能登录本地控制台”

		/*为什么RunAsAdmin需要在获得管理员权限后开一个新的进程
			1.Windows权限是进程级的

			2.通过 LogonUser 获取的管理员令牌（hToken）只能用于创建新进程，无法直接提升当前进程权限。
			绕过UAC限制

			3.直接动态提权会被UAC拦截，而 CreateProcessWithTokenW 能以管理员身份启动新进程，无需用户确认（需配置正确）。
			安全隔离

			4.避免当前进程（普通权限）和特权代码混合，降低风险。

			一句话总结：Windows要求通过新进程承载管理员权限，无法原地升级。
		*/

		STARTUPINFO si = { 0 };  // 定义启动信息结构体，初始化为0
		PROCESS_INFORMATION pi = { 0 };  // 定义进程信息结构体，初始化为0
		TCHAR sPath[MAX_PATH] = _T("");  // 定义字符数组，用于存储当前目录路径，初始化为空字符串
		GetModuleFileName(NULL, sPath, MAX_PATH);
		BOOL ret = CreateProcessWithLogonW(
			_T("Administrator"),  // 指定管理员账户
			NULL,                 // 域名（NULL表示本地计算机）
			NULL,                 // 密码（NULL可能依赖环境配置）
			LOGON_WITH_PROFILE,   // 加载用户配置文件
			NULL,                 // 应用程序名（使用命令行参数）
			sPath,  // 命令行（转换为宽字符指针）
			CREATE_UNICODE_ENVIRONMENT,  // 使用Unicode环境
			NULL,                 // 环境块（NULL表示继承父进程）
			NULL,                 // 当前目录（NULL表示继承父进程）
			&si,                   // 接收启动信息
			&pi                   // 接收进程信息
		);

		if (!ret) {  // 如果创建进程失败，则执行以下操作
			ShowError();  // 显示错误信息
			MessageBox(NULL, sPath, _T("创建进程失败"), 0);  // 弹出消息框，显示创建进程失败的信息
			::exit(0);  // 退出程序
		}
		WaitForSingleObject(pi.hProcess, INFINITE);  // 等待新创建的进程结束
		CloseHandle(pi.hProcess);  // 关闭进程句柄
		CloseHandle(pi.hThread);  // 关闭线程句柄
		return true;

	}

	static void ShowError()
	{
		LPWSTR lpMessageBuf = NULL;
		//strerror(errno);//标准C语言库
		FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			NULL, GetLastError(),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&lpMessageBuf, 0, NULL);
		OutputDebugString(lpMessageBuf);
		MessageBox(NULL, lpMessageBuf, _T("发生错误"), MB_OK);

		LocalFree(lpMessageBuf);
	}

	static BOOL WriteStartupDir(const CString& strPath) {
		// 通过修改开机启动文件夹的内容来实现开机启动
		TCHAR sPath[MAX_PATH] = _T("");
		GetModuleFileName(NULL, sPath, MAX_PATH);

		return CopyFile(sPath, strPath, FALSE);
	}

	/*
	开机启动时，程序执行的权限是跟随启动用户的权限的；如果两者不一致，就会启动失败。（属性-链接器-清单文件-UAC执行级别-asInvoker）
	开机启动对环境变量有影响，如果依赖dll，则可能启动失败：
	1.可以编译时设置为 依据MFC静态库的方式编译（属性-高级-高级属性-MFC的使用-在静态库中使用MFC）
	2. 或者复制对应dll文件到system32或者SysWOW64下面
	反常识的：system32多是64位程序，而SysWOW64下面多是32位程序
	*/
	static bool WriteRegisterTable(const CString& strPath) {
		// 通过修改注册表实现开机启动
		CString strSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"); // 注册表路径，自己到目标电脑上找
		TCHAR sPath[MAX_PATH] = _T("");
		GetModuleFileName(NULL, sPath, MAX_PATH);

		BOOL ret = CopyFile(sPath, strPath, FALSE);
		if (ret == FALSE) {
			MessageBox(NULL, _T("复制文件失败，是否权限不足?\r\n"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
			return false;
		}

		//从根键HKEY_LOCAL_MACHINE打开子键strSubKey
		HKEY hKey = NULL;
		ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, strSubKey, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey);
		// 打开子键strSubKey失败
		if (ret != ERROR_SUCCESS) {
			RegCloseKey(hKey);
			MessageBox(NULL, _T("设置自动开机启动失败！是否权限不足？\r\n程序启动失败！"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
			return false;
		}
		// 打开子键strSubKey成功 则设置注册表项
		ret = RegSetValueEx(hKey, _T("RemoteCtrl"), 0, REG_EXPAND_SZ, (BYTE*)(LPCTSTR)strPath, strPath.GetLength() * sizeof(TCHAR));
		if (ret != ERROR_SUCCESS) {
			RegCloseKey(hKey);
			MessageBox(NULL, _T("设置自动开机启动失败！是否权限不足？\r\n程序启动失败！"), _T("错误"), MB_ICONERROR | MB_TOPMOST);
			return false;
		}
		RegCloseKey(hKey);
		return true;
	}

	static bool Init() {
		// 用于带mfc命令行项目的通用初始化方式
		HMODULE hModule = ::GetModuleHandle(nullptr);
		if (hModule != nullptr) {
			wprintf(L"错误: GetModuleHandle 失败\n");
			return false;
		}

		if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
		{
			wprintf(L"错误: MFC 初始化失败\n");
			return false;

		}
		return true;
	}
};

