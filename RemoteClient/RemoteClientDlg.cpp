
// RemoteClientDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "RemoteClient.h"
#include "RemoteClientDlg.h"
#include "afxdialogex.h"
#include "ClientController.h"
#include "CWatchDialog.h"


#ifdef _DEBUG
#define new DEBUG_NEW // 调试模式下使用 DEBUG_NEW 检测内存泄漏
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
public:
	
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CRemoteClientDlg 对话框


// 构造函数 & 成员变量​初始化
CRemoteClientDlg::CRemoteClientDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_REMOTECLIENT_DIALOG, pParent)
	, m_server_address(0)  // 服务器IP（初始0）
	, m_nPort(_T(""))      // 端口号（初始空）
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);  // 加载程序图标
}

// 数据交换（DDX）
void CRemoteClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_IPAddress(pDX, IDC_IPADDRESS_SERV, m_server_address);  // IP控件绑定
	DDX_Text(pDX, IDC_EDIT_PORT, m_nPort);  // 端口编辑框绑定
	DDX_Control(pDX, IDC_TREE_DIR, m_Tree);  // 目录树控件绑定
	DDX_Control(pDX, IDC_LIST_FILE, m_List);  // 文件列表控件绑定
}

// 消息映射表​
BEGIN_MESSAGE_MAP(CRemoteClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()      // 系统菜单消息
	ON_WM_PAINT()           // 绘制消息
	ON_WM_QUERYDRAGICON()   // 拖动图标消息
	ON_BN_CLICKED(IDC_BTN_TEST, &CRemoteClientDlg::OnBnClickedBtnTest)  // "测试"按钮
	ON_BN_CLICKED(IDC_BTN_FILEINFO, &CRemoteClientDlg::OnBnClickedBtnFileinfo)  // "获取文件信息"按钮
	ON_NOTIFY(NM_DBLCLK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMDblclkTreeDir)  // 树控件双击
	ON_NOTIFY(NM_CLICK, IDC_TREE_DIR, &CRemoteClientDlg::OnNMClickTreeDir)  // 树控件单击
	ON_NOTIFY(NM_RCLICK, IDC_LIST_FILE, &CRemoteClientDlg::OnNMRClickListFile)  // 列表控件右键
	ON_COMMAND(ID_DOWNLOAD_FILE, &CRemoteClientDlg::OnDownloadFile)  // 下载文件菜单项
	ON_COMMAND(ID_DELETE_FILE, &CRemoteClientDlg::OnDeleteFile)  // 删除文件菜单项
	ON_COMMAND(ID_RUN_FILE, &CRemoteClientDlg::OnRunFile)  // 运行文件菜单项
	ON_BN_CLICKED(IDC_BTN_START_WATCH, &CRemoteClientDlg::OnBnClickedBtnStartWatch)  // "开始监控"按钮
	ON_WM_TIMER()  // 定时器消息
	ON_NOTIFY(IPN_FIELDCHANGED, IDC_IPADDRESS_SERV, &CRemoteClientDlg::OnIpnFieldchangedIpaddressServ)  // IP控件变化
	ON_EN_CHANGE(IDC_EDIT_PORT, &CRemoteClientDlg::OnEnChangeEditPort)  // 端口编辑框变化
END_MESSAGE_MAP()

// CRemoteClientDlg 消息处理程序

// 初始化对话框 OnInitDialog()
BOOL CRemoteClientDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();// 基类初始化

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	// 添加"关于"菜单到系统菜单
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX); // 加载"关于"菜单文本
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动执行此操作。
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// 初始化默认IP和端口
	UpdateData();
	m_server_address = 0x0A00020F; // 10.0.2.15（需根据实际修改）
	m_nPort = _T("9527");
	CClientController* pController = CClientController::getInstance();
	pController->UpdataAddress(m_server_address, atoi((LPCTSTR)m_nPort));
	UpdateData(FALSE);

	// 创建状态对话框（隐藏）
	m_dlgStatus.Create(IDD_DLG_STATUS, this);
	m_dlgStatus.ShowWindow(SW_HIDE);
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 系统菜单处理 OnSysCommand()
void CRemoteClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX) {  // 如果是"关于"菜单
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();  // 显示"关于"对话框
	}
	else {
		CDialogEx::OnSysCommand(nID, lParam);  // 其他系统菜单交给基类处理
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，这将由框架自动完成。

// 绘制对话框 OnPaint()
void CRemoteClientDlg::OnPaint()
{
	if (IsIconic()) {  // 如果窗口最小化
		CPaintDC dc(this);  // 获取绘图设备上下文
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);  // 擦除背景

		// 计算图标位置并绘制
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;
		dc.DrawIcon(x, y, m_hIcon);
	}
	else {
		CDialogEx::OnPaint();  // 非最小化状态由基类处理
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标显示。
HCURSOR CRemoteClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


// 测试按钮响应函数
void CRemoteClientDlg::OnBnClickedBtnTest()
{
	CClientController::getInstance()->SendCommandPacket(1981); // 发送测试命令（1981）
}

// 获取文件信息
void CRemoteClientDlg::OnBnClickedBtnFileinfo()
{
	std::list<CPacket> lstPackets;
	int ret = CClientController::getInstance()->SendCommandPacket(1, true, NULL, 0, &lstPackets);
	if (ret == -1 || lstPackets.empty()) {
		AfxMessageBox(_T("命令处理失败!!!"));
		return;
	}

	// 解析服务器返回的驱动器列表
	CPacket& head = lstPackets.front();
	std::string drivers = head.strData;
	std::string dr;
	m_Tree.DeleteAllItems();  // 清空树控件

	// 遍历驱动器列表（格式：C,D,E,...）
	for (size_t i = 0; i < drivers.size(); i++) {
		if (drivers[i] == ',') {
			dr += ":";  // 添加冒号（如 C:）
			HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);  // 插入树控件
			m_Tree.InsertItem(NULL, hTemp, TVI_LAST);  // 插入空子项（用于展开）
			dr.clear();
			continue;
		}
		dr += drivers[i];
	}

	// 处理最后一个驱动器
	if (!dr.empty()) {
		dr += ":";
		HTREEITEM hTemp = m_Tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
		m_Tree.InsertItem(NULL, hTemp, TVI_LAST);
	}
}

// 加载当前目录文件
void CRemoteClientDlg::LoadFileCurrent()
{
	HTREEITEM hTree = m_Tree.GetSelectedItem();  // 获取选中节点
	CString strPath = GetPath(hTree);  // 获取完整路径
	m_List.DeleteAllItems();  // 清空列表

	// 发送请求获取文件列表。SendCommandPacket(2)​​：请求服务器返回指定路径的文件列表。
	int nCmd = CClientController::getInstance()->SendCommandPacket(2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength());
	PFILEINFO pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();

	// 遍历文件列表
	while (pInfo->HasNext) {
		TRACE("[%s] isdir %d\r\n", pInfo->szFileName, pInfo->IsDirectory);
		if (!pInfo->IsDirectory) {
			m_List.InsertItem(0, pInfo->szFileName);  // 非目录则插入列表
		}
		int cmd = CClientController::getInstance()->DealCommand();  // 处理下一个文件
		if (cmd < 0) break;
		pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
	}
}

// 加载文件信息​
void CRemoteClientDlg::LoadFileInfo()
{
	/*
	当用户点击树控件的某个节点时，加载该路径下的文件和子目录。
	如果是目录，则在树控件中显示子目录；如果是文件，则在列表控件中显示。
	*/
	// 获取鼠标位置并转换为树控件坐标
	CPoint ptMouse;
	GetCursorPos(&ptMouse);
	m_Tree.ScreenToClient(&ptMouse);

	// 获取鼠标点击的树节点
	HTREEITEM hTreeSelected = m_Tree.HitTest(ptMouse, 0);
	if (hTreeSelected == NULL) return;  // 未选中节点则退出

	// 如果该节点没有子节点，则退出（避免重复加载）
	if (m_Tree.GetChildItem(hTreeSelected) == NULL) return;

	// 清空该节点的所有子节点
	DeleteTreeChildrenItem(hTreeSelected);

	// 清空文件列表
	m_List.DeleteAllItems();

	// 获取完整路径（如 "C:\Windows\"）
	CString strPath = GetPath(hTreeSelected);

	// 发送命令获取该路径下的文件列表
	std::list<CPacket> lstPackets;
	int nCmd = CClientController::getInstance()->SendCommandPacket(
		2,  // 命令码2：获取文件列表
		false,
		(BYTE*)(LPCTSTR)strPath,
		strPath.GetLength(),
		&lstPackets
	);

	// 遍历返回的文件信息
	if (lstPackets.size() > 0) {
		auto it = lstPackets.begin();
		for (; it != lstPackets.end(); it++) {
			PFILEINFO pInfo = (PFILEINFO)(*it).strData.c_str();
			if (pInfo->HasNext == FALSE) continue;  // 无效数据跳过

			if (pInfo->IsDirectory) {  // 如果是目录
				// 跳过 "." 和 ".."（当前目录和上级目录）
				if (CString(pInfo->szFileName) == "." || CString(pInfo->szFileName) == "..") {
					continue;
				}
				// 在树控件中添加目录节点
				HTREEITEM hTemp = m_Tree.InsertItem(pInfo->szFileName, hTreeSelected, TVI_LAST);
				m_Tree.InsertItem("", hTemp, TVI_LAST);  // 添加空子节点（用于展开）
			}
			else {  // 如果是文件
				m_List.InsertItem(0, pInfo->szFileName);  // 在列表控件中添加文件名
			}
		}
	}
}

//  获取完整路径​
CString CRemoteClientDlg::GetPath(HTREEITEM hTree)
{
	/*从树控件的某个节点开始，递归获取完整路径（如 "C:\Windows\System32\"）。*/
	CString strRet, strTmp;
	do {
		strTmp = m_Tree.GetItemText(hTree);  // 获取当前节点文本（如 "Windows"）
		strRet = strTmp + '\\' + strRet;     // 拼接路径（如 "Windows\System32\"）
		hTree = m_Tree.GetParentItem(hTree); // 获取父节点
	} while (hTree != NULL);  // 递归直到根节点
	return strRet;
}

// 删除树节点的子项​
void CRemoteClientDlg::DeleteTreeChildrenItem(HTREEITEM hTree)
{
	HTREEITEM hSub = NULL;
	do {
		hSub = m_Tree.GetChildItem(hTree);  // 获取第一个子节点
		if (hSub != NULL) m_Tree.DeleteItem(hSub);  // 删除子节点
	} while (hSub != NULL);  // 循环直到没有子节点
}


// 树控件双击事件
void CRemoteClientDlg::OnNMDblclkTreeDir(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
	LoadFileInfo();
}

// 树控件单击事件​
void CRemoteClientDlg::OnNMClickTreeDir(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
	LoadFileInfo();
}

// 列表控件右键事件​
void CRemoteClientDlg::OnNMRClickListFile(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	*pResult = 0;

	// 获取鼠标位置
	CPoint ptMouse, ptList;
	GetCursorPos(&ptMouse);
	ptList = ptMouse;
	m_List.ScreenToClient(&ptList);

	// 检查是否选中了文件
	int ListSelected = m_List.HitTest(ptList);
	if (ListSelected < 0) return;  // 未选中则退出

	// 加载右键菜单
	CMenu menu;
	menu.LoadMenu(IDR_MENU_RCLICK);  // 加载菜单资源
	CMenu* pPopup = menu.GetSubMenu(0);
	if (pPopup != NULL) {
		pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, ptMouse.x, ptMouse.y, this);
	}
}

// 下载文件​
void CRemoteClientDlg::OnDownloadFile()
{
	// 获取选中的文件
	int nListSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nListSelected, 0);

	// 拼接完整路径（如 "C:\Windows\notepad.exe"）
	HTREEITEM hSelected = m_Tree.GetSelectedItem();
	strFile = GetPath(hSelected) + strFile;

	// 调用下载功能
	int ret = CClientController::getInstance()->DownFile(strFile);
	if (ret != 0) {
		MessageBox(_T("下载文件失败！！！"));
		TRACE("执行下载失败：ret = %d\r\n", ret);
	}
}

// 删除文件​
void CRemoteClientDlg::OnDeleteFile()
{
	// 获取选中的文件
	HTREEITEM hSelected = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hSelected);
	int nSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nSelected, 0);
	strFile = strPath + strFile;  // 拼接完整路径

	// 发送删除命令
	int ret = CClientController::getInstance()->SendCommandPacket(
		9,  // 命令码9：删除文件
		true,
		(BYTE*)(LPCSTR)strFile,
		strFile.GetLength()
	);

	if (ret < 0) {
		AfxMessageBox("删除文件命令执行失败！！！");
	}
	LoadFileCurrent();  // 刷新文件列表
}

// 运行文件​
void CRemoteClientDlg::OnRunFile()
{
	// 获取选中的文件
	HTREEITEM hSelected = m_Tree.GetSelectedItem();
	CString strPath = GetPath(hSelected);
	int nSelected = m_List.GetSelectionMark();
	CString strFile = m_List.GetItemText(nSelected, 0);
	strFile = strPath + strFile;  // 拼接完整路径

	// 发送运行命令
	int ret = CClientController::getInstance()->SendCommandPacket(
		3,  // 命令码3：运行文件
		true,
		(BYTE*)(LPCSTR)strFile,
		strFile.GetLength()
	);

	if (ret < 0) {
		AfxMessageBox("打开文件命令执行失败！！！");
	}
}

// 启动远程监控
void CRemoteClientDlg::OnBnClickedBtnStartWatch()
{
	CClientController::getInstance()->StartWatchScreen();
}


void CRemoteClientDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值

	CDialogEx::OnTimer(nIDEvent);
}

// IP 控件变化事件​
void CRemoteClientDlg::OnIpnFieldchangedIpaddressServ(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMIPADDRESS pIPAddr = reinterpret_cast<LPNMIPADDRESS>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
	UpdateData();
	CClientController* pController = CClientController::getInstance();
	pController->UpdataAddress(m_server_address, atoi((LPCTSTR)m_nPort));
}

// 端口编辑框变化事件​
void CRemoteClientDlg::OnEnChangeEditPort()
{
	UpdateData();
	CClientController* pController = CClientController::getInstance();
	pController->UpdataAddress(m_server_address, atoi((LPCTSTR)m_nPort));
}
