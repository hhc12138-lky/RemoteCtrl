// CWatchDialog.cpp: 实现文件
//

#include "pch.h"
#include "RemoteClient.h"
#include "CWatchDialog.h"
#include "afxdialogex.h"
#include "ClientController.h"


// CWatchDialog 对话框

// IMPLEMENT_DYNAMIC​​：MFC 宏，用于运行时类型识别（RTTI），允许动态创建对话框类。
IMPLEMENT_DYNAMIC(CWatchDialog, CDialog)

CWatchDialog::CWatchDialog(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_DLG_WATCH, pParent)
{
	m_isFull = false;    // 是否全屏标志，初始化为 false
	m_nObjWidth = -1;    // 远程屏幕宽度，初始化为 -1（未设置）
	m_nObjHeight = -1;   // 远程屏幕高度，初始化为 -1（未设置）
}

CWatchDialog::~CWatchDialog()
{
}

void CWatchDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);  // 调用基类数据交换
	DDX_Control(pDX, IDC_WATCH, m_picture);  // 关联 IDC_WATCH 控件到 m_picture 变量
}


// 消息映射表​
BEGIN_MESSAGE_MAP(CWatchDialog, CDialog)
	ON_WM_TIMER()                // 定时器消息
	ON_WM_LBUTTONDBLCLK()        // 左键双击
	ON_WM_LBUTTONDOWN()          // 左键按下
	ON_WM_LBUTTONUP()            // 左键释放
	ON_WM_RBUTTONDBLCLK()        // 右键双击
	ON_WM_RBUTTONDOWN()          // 右键按下
	ON_WM_RBUTTONUP()            // 右键释放
	ON_WM_MOUSEMOVE()            // 鼠标移动
	ON_STN_CLICKED(IDC_WATCH, &CWatchDialog::OnStnClickedWatch)  // 图片控件点击
	ON_BN_CLICKED(IDC_BTN_LOCK, &CWatchDialog::OnBnClickedBtnLock)    // "锁定"按钮点击
	ON_BN_CLICKED(IDC_BTN_UNLOCK, &CWatchDialog::OnBnClickedBtnUnlock) // "解锁"按钮点击
	ON_MESSAGE(WM_SEND_PACK_ACK,&CWatchDialog::OnSendPackAck)
END_MESSAGE_MAP()


// CWatchDialog 消息处理程序

// 坐标转换函数​
CPoint CWatchDialog::UserPoint2RemoteScreenPoint(CPoint& point, bool isScreen)
{
	CRect clientRect;
	if (!isScreen) {
		ClientToScreen(&point); // 转换为相对屏幕坐上角的坐标（屏幕内的绝对坐标）
	}
	m_picture.ScreenToClient(&point);  // 全局坐标 → 客户区坐标

	TRACE("x=%d y=%d\r\n", point.x, point.y);  // 调试输出坐标

	m_picture.GetWindowRect(clientRect);  // 获取图片控件的窗口矩形
	TRACE("x=%d y=%d\r\n", clientRect.Width(), clientRect.Height());  // 调试输出控件尺寸

	// 本地坐标 → 远程坐标（按比例缩放）
	return CPoint(
		point.x * m_nObjWidth / clientRect.Width(),
		point.y * m_nObjHeight / clientRect.Height()
	);
}

BOOL CWatchDialog::OnInitDialog()
{
	CDialog::OnInitDialog();  // 调用基类初始化
	m_isFull = false;         // 初始非全屏模式
	//SetTimer(0, 45, NULL);    // 启动定时器（ID=0，间隔45ms）
	return TRUE;  // 返回 TRUE 表示初始化成功
}

// 定时器处理（屏幕更新）​
void CWatchDialog::OnTimer(UINT_PTR nIDEvent)
{
	//if (nIDEvent == 0) {  // 检查定时器ID
	//	CClientController* pParent = CClientController::getInstance();
	//	if (m_isFull) {  // 如果处于全屏模式
	//		CRect rect;
	//		m_picture.GetWindowRect(rect);  // 获取图片控件尺寸
	//		m_nObjWidth = m_image.GetWidth();   // 获取远程图像宽度
	//		m_nObjHeight = m_image.GetHeight(); // 获取远程图像高度

	//		// 拉伸图像到控件大小
	//		m_image.StretchBlt(
	//			m_picture.GetDC()->GetSafeHdc(),  // 目标设备上下文
	//			0, 0, rect.Width(), rect.Height(), // 目标位置和尺寸
	//			SRCCOPY  // 直接复制
	//		);
	//		m_picture.InvalidateRect(NULL);  // 强制重绘
	//		m_image.Destroy();  // 销毁图像
	//		m_isFull = false;   // 重置全屏标志
	//	}
	//}
	CDialog::OnTimer(nIDEvent);  // 调用基类处理
}

LRESULT CWatchDialog::OnSendPackAck(WPARAM wParam, LPARAM lParam)
{
	if (lParam == -1 || lParam == -2) {
		//TODO：错误处理
	}
	else if (lParam == 1) {
		// 对方关闭了套接字
	}else {
		if (wParam != NULL) {
			CPacket head = *(CPacket*)wParam;
			delete (CPacket*)wParam;; // 为了数据包的跨线程通信，发送线程那边制造了个堆对象，在这里获得后需要释放内存

			switch (head.sCmd) {
			case 6:
			{
				CEdoyunTool::Bytes2Image(m_image, head.strData);
				CRect rect;
				m_picture.GetWindowRect(rect);  // 获取图片控件尺寸
				m_nObjWidth = m_image.GetWidth();   // 获取远程图像宽度
				m_nObjHeight = m_image.GetHeight(); // 获取远程图像高度

				// 拉伸图像到控件大小
				m_image.StretchBlt(
					m_picture.GetDC()->GetSafeHdc(),  // 目标设备上下文
					0, 0, rect.Width(), rect.Height(), // 目标位置和尺寸
					SRCCOPY  // 直接复制
				);
				m_picture.InvalidateRect(NULL);  // 强制重绘
				m_image.Destroy();  // 销毁图像
				break;
			}
			case 5:
				TRACE("远程端应答了鼠标操作\r\n");
				break;
			case 7:
			case 8:
			default:
				break;
			}
		}
	}
	return 0;
}

// 左键双击​
void CWatchDialog::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {  // 检查是否已获取远程分辨率
		TRACE("point: x=%d y=%d \r\n", point.x, point.y);
		CPoint remote = UserPoint2RemoteScreenPoint(point);  // 坐标转换
		TRACE("point after convert: x=%d y=%d \r\n", point.x, point.y);
		TRACE("remote after convert: x=%d y=%d \r\n", remote.x, remote.y);

		MOUSEEV event;
		event.ptXY = remote;  // 远程坐标
		event.nButton = 0;    // 左键
		event.nAction = 2;    // 双击
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnLButtonDblClk(nFlags, point);
}

// 左键按下
void CWatchDialog::OnLButtonDown(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;  // 左键
		event.nAction = 2;  // 按下（TODO：服务端可能需要修改）
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnLButtonDown(nFlags, point);
}

// 左键释放​
void CWatchDialog::OnLButtonUp(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;  // 左键
		event.nAction = 3;  // 释放
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
	CDialog::OnLButtonUp(nFlags, point);
}

// 右键双击
void CWatchDialog::OnRButtonDblClk(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;//左键
		event.nAction = 1;//双击
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));

	}
	CDialog::OnRButtonDblClk(nFlags, point);
}

// 右键按下
void CWatchDialog::OnRButtonDown(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;//左键
		event.nAction = 2;//按下 //TODO:服务端要做对应的修改
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));

	}
	CDialog::OnRButtonDown(nFlags, point);
}

// 右键释放
void CWatchDialog::OnRButtonUp(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 1;//左键
		event.nAction = 3;//弹起
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));

	}
	CDialog::OnRButtonUp(nFlags, point);
}

// 移动
void CWatchDialog::OnMouseMove(UINT nFlags, CPoint point)
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		//坐标转换
		CPoint remote = UserPoint2RemoteScreenPoint(point);
		//封装
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 8;//没有按键
		event.nAction = 0;//移动
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));

	}
	CDialog::OnMouseMove(nFlags, point);
}

// 图片控件点击
void CWatchDialog::OnStnClickedWatch()
{
	if ((m_nObjWidth != -1) && (m_nObjHeight != -1)) {
		CPoint point;
		GetCursorPos(&point);  // 获取鼠标全局坐标
		CPoint remote = UserPoint2RemoteScreenPoint(point, true);  // 转换为远程坐标
		MOUSEEV event;
		event.ptXY = remote;
		event.nButton = 0;  // 左键
		event.nAction = 0;  // 单击
		CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 5, true, (BYTE*)&event, sizeof(event));
	}
}


void CWatchDialog::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialog::OnOK();
}

//锁定远程计算机​
void CWatchDialog::OnBnClickedBtnLock()
{
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 7);

}

//解锁远程计算机​
void CWatchDialog::OnBnClickedBtnUnlock()
{
	CClientController::getInstance()->SendCommandPacket(GetSafeHwnd(), 8);

}
