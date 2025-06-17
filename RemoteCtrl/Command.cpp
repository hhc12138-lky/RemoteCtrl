#include "pch.h"
#include "Command.h"

CCommand::CCommand() :threadid(0)
{
	// 指令号与函数指针的映射
	struct {
		int nCmd;
		CMDFUNC func;
	}data[] = {
		{ 1, &CCommand::MakeDriverInfo},
		{ 2, &CCommand::MakeDirectoryInfo },
		{ 3, &CCommand::RunFile },
		{ 4, &CCommand::DownloadFile },
		{ 5, &CCommand::MouseEvent },
		{ 6, &CCommand::SendScreen },
		{ 7, &CCommand::LockMachine },
		{ 8, &CCommand::UnlockMachine },
		{ 9, &CCommand::DeleteLocalFile },
		{ 1981, &CCommand::TestConnect },
	};
	for (const auto& item : data) {
		m_mapFunction[item.nCmd] = item.func;
	}
}

CCommand::~CCommand() {
	// 清理资源（如有）
}

// 线程回调执行最终的端点：命令的执行
int CCommand::ExcuteCommand(int nCmd, std::list<CPacket>& listPackets, CPacket& inPacket)
{
	//根据指令号从映射表中寻找成员函数指针，再根据该指针调用成员函数
	std::map<int, CMDFUNC>::iterator it = m_mapFunction.find(nCmd);
	if (it == m_mapFunction.end()) {
		return -1;
	}

	TRACE("Executing command: %d\n", nCmd);
	return(this->*it->second)(listPackets, inPacket);

	/*  this->*it->second解释
	C++中，这些操作符的优先级如下（从高到低）：
	->  (成员访问)  >  
	->* (成员指针访问)

	注意到 ->* 是一个​​整体操作符​​（成员指针访问操作符），用于将成员函数指针绑定到对象（this）！

	所以解析步骤为：
	先计算 it->second (访问map值)
	最后计算 ->* (成员函数指针绑定)
	*/
}
