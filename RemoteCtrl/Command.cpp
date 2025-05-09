#include "pch.h"
#include "Command.h"

CCommand::CCommand() :threadid(0)
{
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
	for (int i = 0; data[i].nCmd != 0; i++)
	{
		m_mapFunction[data[i].nCmd] = data[i].func;
	}
}

CCommand::~CCommand() {
	// 清理资源（如有）
}

int CCommand::ExcuteCommand(int nCmd)
{
	//根据指令号从映射表中寻找成员函数指针，再根据该指针调用成员函数
	std::map<int, CMDFUNC>::iterator it = m_mapFunction.find(nCmd);
	if (it == m_mapFunction.end()) {
		return -1;
	}
	
	return(this->*it->second)();

}
