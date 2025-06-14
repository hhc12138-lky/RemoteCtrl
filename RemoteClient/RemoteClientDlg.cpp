
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
	ON_MESSAGE(WM_SEND_PACK_ACK, &CRemoteClientDlg::OnSendPackAck)

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
	// 初始化UI数据
	InitUIData();
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
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 1981); // 发送测试命令（1981）
}

// 获取文件信息
void CRemoteClientDlg::OnBnClickedBtnFileinfo()
{
	std::list<CPacket> lstPackets;
	bool ret = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 1, true, NULL, 0);
	if (ret == 0) {
		AfxMessageBox(_T("命令处理失败!!!"));
		return;
	}

}

void CRemoteClientDlg::DealCommand(WORD nCmd, const std::string& strData, LPARAM lParam)
{
	switch (nCmd) {
	case 1:// 获取驱动信息
		Str2Tree(strData, m_Tree);
		break;
	case 2://文件信息
		UpdataFileInfo(*(PFILEINFO)strData.c_str(), (HTREEITEM)lParam);
		break;
	case 3:
		MessageBox("打开文件完成！", "操作完成", MB_ICONINFORMATION);
		break;
	case 4:
		UpdataDownloadFile(strData, (FILE*)lParam);
		break;
	case 9:
		MessageBox("删除文件操作已执行！", "操作已执行", MB_ICONINFORMATION);
		break;
	case 1981:
		MessageBox("连接测试成功！", "连接成功", MB_ICONINFORMATION);
		break;
	default:
		TRACE("unknow data recieved!%d \r\n", nCmd);
		break;
	}
}

void CRemoteClientDlg::InitUIData()
{
	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动执行此操作。
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标
	// 初始化默认IP和端口
	UpdateData();

	// 1.本地测试用
	//m_server_address = 0x0A00020F; // 10.0.2.15（需根据实际修改）
	m_server_address = 0x7F000001; // 127.0.0.1（本地测试用）
	m_nPort = _T("9527");

	//2.虚拟机测试用 注意我的虚拟机使用NAT+端口转发实现内部网络访问 目前本机的127.0.0.1:11528 映射到虚拟机网络的127.0.0.1:9527
	//m_server_address = 0x7F000001; // 127.0.0.1（本地测试用）
	//m_nPort = _T("11528");

	CClientController* pController = CClientController::getInstance();
	pController->UpdataAddress(m_server_address, atoi((LPCTSTR)m_nPort));
	UpdateData(FALSE);

	// 创建状态对话框（隐藏）
	m_dlgStatus.Create(IDD_DLG_STATUS, this);
	m_dlgStatus.ShowWindow(SW_HIDE);
}

// 加载当前目录文件
void CRemoteClientDlg::LoadFileCurrent()
{
	HTREEITEM hTree = m_Tree.GetSelectedItem();  // 获取选中节点
	CString strPath = GetPath(hTree);  // 获取完整路径
	m_List.DeleteAllItems();  // 清空列表

	// 发送请求获取文件列表。SendCommandPacket(2)​​：请求服务器返回指定路径的文件列表。
	int nCmd = CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 2, false, (BYTE*)(LPCTSTR)strPath, strPath.GetLength());
	PFILEINFO pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();

	// 遍历文件列表
	while (pInfo->HasNext) {
		TRACE("[%s] isdir %d\r\n", pInfo->szFileName, pInfo->IsDirectory);
		if (!pInfo->IsDirectory) {
			m_List.InsertItem(0, pInfo->szFileName);  // 非目录则插入列表
		}
		int cmd = CClientController::getInstance()->DealCommand(); 
		if (cmd < 0) break;
		pInfo = (PFILEINFO)CClientSocket::getInstance()->GetPacket().strData.c_str();
	}
}

void CRemoteClientDlg::Str2Tree(const std::string& drivers, CTreeCtrl& tree)
{
	// 解析服务器返回的驱动器列表
	std::string dr;
	tree.DeleteAllItems();  // 清空树控件

	// 遍历驱动器列表（格式：C,D,E,...）
	for (size_t i = 0; i < drivers.size(); i++) {
		if (drivers[i] == ',') {
			dr += ":";  // 添加冒号（如 C:）
			HTREEITEM hTemp = tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);  // 插入树控件
			tree.InsertItem(NULL, hTemp, TVI_LAST);  // 插入空子项（用于展开）
			dr.clear();
			continue;
		}
		dr += drivers[i];
	}

	// 处理最后一个驱动器
	if (!dr.empty()) {
		dr += ":";
		HTREEITEM hTemp = tree.InsertItem(dr.c_str(), TVI_ROOT, TVI_LAST);
		tree.InsertItem(NULL, hTemp, TVI_LAST);
	}
}

void CRemoteClientDlg::UpdataFileInfo(const FILEINFO& finfo, HTREEITEM hParent)
{
	if (finfo.HasNext == FALSE) return;  // 无效数据跳过

	if (finfo.IsDirectory) {  // 如果是目录
		// 跳过 "." 和 ".."（当前目录和上级目录）
		if (CString(finfo.szFileName) == "." || CString(finfo.szFileName) == "..") {
			return;
		}
		// 在树控件中添加目录节点
		HTREEITEM hTemp = m_Tree.InsertItem(finfo.szFileName, hParent, TVI_LAST);
		m_Tree.InsertItem("", hTemp, TVI_LAST);  // 添加空子节点（用于展开）
		m_Tree.Expand(hParent, TVE_EXPAND);
	}
	else {  // 如果是文件
		m_List.InsertItem(0, finfo.szFileName);  // 在列表控件中添加文件名
	}
}

void CRemoteClientDlg::UpdataDownloadFile(const std::string& strData, FILE* pFile)
{
	static LONGLONG length = 0, index = 0;
	length = *(long long*)strData.c_str();
	if (length == 0) {
		AfxMessageBox("文件长度为零或者无法读取文件！！！");
		CClientController::getInstance()->DownloadEnd();
	}
	else if (length > 0 && (index >= length)) {
		fclose(pFile);
		length = 0;
		index = 0;
		CClientController::getInstance()->DownloadEnd();

	}
	else {
		fwrite(strData.c_str(), 1,strData.size(), pFile);
		index += strData.size();

		// 判断是否读取完成
		if (index >= length) {
			fclose(pFile);
			length = 0;
			index = 0;
			CClientController::getInstance()->DownloadEnd();
		}
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


	// 清空该节点的所有子节点
	DeleteTreeChildrenItem(hTreeSelected);

	// 清空文件列表
	m_List.DeleteAllItems();

	// 获取完整路径（如 "C:\Windows\"）
	CString strPath = GetPath(hTreeSelected);

	// 发送命令获取该路径下的文件列表
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(),2, false,(BYTE*)(LPCTSTR)strPath,strPath.GetLength(), (WPARAM)hTreeSelected);

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
		GetSafeHwnd(),
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
		GetSafeHwnd(),
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

LRESULT CRemoteClientDlg::OnSendPackAck(WPARAM wParam, LPARAM lParam)
{

	if (lParam == -1 || lParam == -2) {
		//TODO：错误处理
		TRACE("socket is error %d!! \r\n", lParam);
	}
	else if (lParam == 1) {
		// 对方关闭了套接字
		TRACE("socket is closed!! \r\n");
	}
	else {
		if (wParam != NULL) {
			CPacket head = *(CPacket*)wParam;
			delete (CPacket*)wParam;; // 为了数据包的跨线程通信，发送线程那边制造了个堆对象，在这里获得后需要释放内存
			DealCommand(head.sCmd, head.strData, lParam);
		}
	}
	return 0;
}
