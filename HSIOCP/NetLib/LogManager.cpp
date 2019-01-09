#include "stdafx.h"
#include "LogManager.h"

namespace NetLib
{
	std::unique_ptr<LogManager> LogManager::m_pInstance;
	std::once_flag LogManager::m_OnceFlag;
}