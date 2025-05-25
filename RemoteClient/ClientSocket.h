#pragma once
#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>
#include <list>
#include <map>
#include <mutex>

#define WM_SEND_PACK (WM_USER+1) // 发送包
#define WM_SEND_PACK_ACK (WM_USER+2) // 发送包数据应答

#pragma pack(push)
#pragma pack(1)

// 协议化的网络数据包 用于网络传输
class CPacket
{
public:
	// 构造函数
	CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0){}
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
		nLength = nSize + 4;
		sCmd = nCmd;
		if (nSize > 0) {
			strData.resize(nSize);
			memcpy((void*)strData.c_str(), pData, nSize);
		}
		else {
			strData.clear();
		}
		sSum = 0;
		for (size_t j = 0; j < strData.size(); j++)
		{
			sSum += BYTE(strData[j]) & 0xFF;
		}
	}
	// 拷贝构造函数
	CPacket(const CPacket& pack) {
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		strData = pack.strData;
		sSum = pack.sSum;
	}
	// 用于解包的构造函数
	CPacket(const BYTE* pData, size_t& nSize){
		//[魔数sHead 2字节][包长度nLength 4字节][命令号sCmd 2字节][有效数据strData N字节][校验和sSum 2字节]
		
		size_t i = 0;
		// 魔数检测（同步数据流）
		for (; i < nSize; i++) {
			if (*(WORD*)(pData + i) == 0xFEFF) {
				sHead = *(WORD*)(pData + i);
				i += 2;
				break;
			}
		}
		// 长度预校验: 包数据可能不全，或者包头未能全部接收到
		if (i + 4 + 2 + 2 > nSize) {
			nSize = 0;
			return;
		}
		// 读取包长度: 包未完全接收到，就返回，解析失败
		nLength = *(DWORD*)(pData + i); i += 4;
		if (nLength + i > nSize) {
			nSize = 0;
			return;
		}
		// 读取命令号
		sCmd = *(WORD*)(pData + i); i += 2;
		if (nLength > 4) {
			// 有效数据长度 = 总长度 - 命令号(2) - 校验和(2); 预先调整字符串大小避免多次分配
			strData.resize(nLength - 2 - 2); 
			memcpy((void*)strData.c_str(), pData + i, nLength - 4);
			//TRACE("%s\r\n", strData.c_str() + 12);
			i += nLength - 4;
		}
		// 校验和验证
		sSum = *(WORD*)(pData + i); i += 2;
		WORD sum = 0;
		for (size_t j = 0; j < strData.size(); j++)
		{
			sum += BYTE(strData[j]) & 0xFF;
		}
		// 结果处理
		if (sum == sSum) {
			// 成功​​：更新 nSize 为已消耗的字节数
			nSize = i;
			return;
		}
		// ​​失败​​：重置 nSize=0 要求重试
		nSize = 0;
	}
	~CPacket() {}
	CPacket& operator=(const CPacket& pack) {
		if (this != &pack) {
			sHead = pack.sHead;
			nLength = pack.nLength;
			sCmd = pack.sCmd;
			strData = pack.strData;
			sSum = pack.sSum;
		}
		return *this;
	}
	int Size() {//包数据的大小
		return nLength + 6;
	}
	const char* Data(std::string& strOut) const {
		// 将数据包序列化为二进制格式以便网络传输 使用传入引用返回
		strOut.resize(nLength + 6);
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData += 2;
		*(DWORD*)(pData) = nLength; pData += 4;
		*(WORD*)pData = sCmd; pData += 2;
		memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
		*(WORD*)pData = sSum;
		return strOut.c_str();
	}

public:
	WORD sHead;//固定值 0xFEFF，用于标识数据包起始位置
	DWORD nLength;//包长度（从控制命令开始，到和校验结束）
	WORD sCmd;//控制命令
	std::string strData;//包数据
	WORD sSum;//和校验
	//std::string strOut;//整个包的数据
};
#pragma pack(pop)

typedef struct MouseEvent {
	MouseEvent() {
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction;//点击、移动、双击
	WORD nButton;//左键、右键、中键
	POINT ptXY;//坐标
}MOUSEEV, * PMOUSEEV;

typedef struct file_info {
	file_info() {
		IsInvalid = FALSE;
		IsDirectory = -1;
		HasNext = TRUE;
		memset(szFileName, 0, sizeof(szFileName));
	}
	BOOL IsInvalid;//是否有效
	BOOL IsDirectory;//是否为目录 0 否 1 是
	BOOL HasNext;//是否还有后续 0 没有 1 有
	char szFileName[256];//文件名
}FILEINFO, * PFILEINFO;

enum {
	CSM_AUTOCLOSE=1//CSM = CLient Socket Mode 自动关闭模式
};

// 轻量级数据容器，通过wParam传递Windows消息参数；作为消息队列中的数据类型，用于线程间通信。
typedef struct PacketData{
	std::string strData;
	UINT nMode;
	WPARAM wParam;
	PacketData(const char* pData, size_t nLen, UINT mode, WPARAM nParam = 0) {
		strData.resize(nLen);
		memcpy((char*)strData.c_str(), pData, nLen);
        nMode = mode;
		wParam = nParam;
	}
	PacketData(const PacketData& data) {
        strData = data.strData;
        nMode = data.nMode;
		wParam = data.wParam;
	}
    PacketData& operator=(const PacketData& data) { 
		if (this != &data) {
            strData = data.strData;
            nMode = data.nMode;
			wParam = data.wParam;

		}
        return *this;
	}
}PACKET_DATA;

std::string GetErrInfo(int wsaErrCode);

void Dump(BYTE* pData, size_t nSize);

class CClientSocket
{
public:
	static CClientSocket* getInstance() {
		if (m_instance == NULL) {//静态函数没有this指针，所以无法直接访问成员变量
			m_instance = new CClientSocket();
		}
		return m_instance;
	}
	bool InitSocket();


#define BUFFER_SIZE 4096000
	int DealCommand();

	//bool SendPacket(const CPacket& pack, std::list<CPacket>& lstPacks, bool isAutoClosed = true);
	bool SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed = true, WPARAM wParam = 0);
	bool GetFilePath(std::string& strPath) {
		//解析包数据为路径 通过引用返回
		if ((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4)) {
			strPath = m_packet.strData;
			return true;
		}
		return false;
	}
	bool GetMouseEvent(MOUSEEV& mouse) {
		//解析包数据为鼠标结构 通过引用返回
		if (m_packet.sCmd == 5) {
			memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
			return true;
		}
		return false;
	}
	CPacket& GetPacket() {
		return m_packet;
	}
	void CloseSocket() {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}

	void UpdataAddress(int nIP, int nPort) {
		if ((m_nIP != nIP) || (m_nPort != nPort)) {
			m_nIP = nIP;
			m_nPort = nPort;
		}
	}
private:
	HANDLE m_eventInvoke;// 线程通信事件
	UINT m_nThreadID;
	typedef void(CClientSocket::* MSGFUNC)(UINT, WPARAM, LPARAM);
	std::map<UINT, MSGFUNC> m_mapFunc;
	HANDLE m_hThread;
	bool m_bAutoClose;
	std::mutex m_lock;
	std::list<CPacket> m_lstSend;
	//应答包映射表：关联事件与返回数据，用于存储不同指令应带回来的不连续的包；对随机取值无要求，因此使用双向列表
	std::map<HANDLE, std::list<CPacket>&> m_mapAck; 
	// 自动关闭映射表：控制连接生命周期，标记每个请求是否需要在响应处理后 ​​自动关闭Socket连接​​。
	std::map<HANDLE, bool> m_mapAutoClosed; 
	int m_nIP;//地址
	int m_nPort;//端口
	std::vector<char> m_buffer;
	SOCKET m_sock;
	CPacket m_packet;
	CClientSocket& operator=(const CClientSocket& ss) {}
	CClientSocket(const CClientSocket& ss);
	CClientSocket();

	~CClientSocket() {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		WSACleanup();
	}

	//发送数据包并处理服务器响应的线程
	static unsigned __stdcall threadEntry(void* arg);
	void threadFunc();
	void threadFunc2();

	BOOL InitSockEnv() {
		WSADATA data;
		if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
			return FALSE;
		}
		return TRUE;
	}
	static void releaseInstance() {
		if (m_instance != NULL) {
			CClientSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
		}
	}
	bool Send(const char* pData, int nSize) {
		if (m_sock == -1)return false;
		return send(m_sock, pData, nSize, 0) > 0;
	}
	bool Send(const CPacket& pack);
	void SendPack(UINT nMSg, WPARAM wParam/*缓冲区的值*/, LPARAM lParam/*缓冲区的长度*/);
	static CClientSocket* m_instance;
	class CHelper {
	public:
		CHelper() {
			CClientSocket::getInstance();
		}
		~CHelper() {
			CClientSocket::releaseInstance();
		}
	};
	static CHelper m_helper;
};