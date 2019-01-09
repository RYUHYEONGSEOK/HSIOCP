#include "stdafx.h"
#include "MessagePool.h"

#include "LogManager.h"

namespace NetLib
{
	MessagePool::MessagePool(const int maxMsgPoolCount, const int extraMsgPoolCount)
		: m_MaxMessagePoolCount(maxMsgPoolCount)
		, m_ExtraMessagePoolCount(extraMsgPoolCount)
	{
		CreateMessagePool();
	}

	MessagePool::~MessagePool()
	{
		DestroyMessagePool();
	}

	bool MessagePool::CheckCreate(void)
	{
		if (m_MaxMessagePoolCount == INVALID_VALUE)
		{
			g_LogMgr.WriteLog(LOG_ERROR, "MessagePool Fail : {} {}", __FUNCTION__, __LINE__);
			return false;
		}

		if (m_ExtraMessagePoolCount == INVALID_VALUE)
		{
			g_LogMgr.WriteLog(LOG_ERROR, "MessagePool Fail : {} {}", __FUNCTION__, __LINE__);
			return false;
		}

		return true;
	}

	bool MessagePool::DeallocMsg(Message* pMsg)
	{
		if (pMsg == nullptr)
		{
			return false;
		}

		pMsg->Clear();

		m_MessagePool.push(pMsg);
		return true;
	}

	Message* MessagePool::AllocMsg(void)
	{
		Message* pMsg = nullptr;
		if (!m_MessagePool.try_pop(pMsg))
		{
			return nullptr;
		}

		return pMsg;
	}

	bool MessagePool::CreateMessagePool(void)
	{
		Message* pMsg = nullptr;
		for (int i = 0; i < m_MaxMessagePoolCount; ++i)
		{
			pMsg = new Message;
			pMsg->Clear();

			m_MessagePool.push(pMsg);
			m_OriginalMessages.push_back(pMsg);
		}

		for (int i = 0; i < m_ExtraMessagePoolCount; ++i)
		{
			pMsg = new Message;
			pMsg->Clear();

			m_MessagePool.push(pMsg);
			m_OriginalMessages.push_back(pMsg);
		}

		return true;
	}

	void MessagePool::DestroyMessagePool(void)
	{
		for (auto oneMessage : m_OriginalMessages)
		{
			SAFE_DELETE(oneMessage);
		}
	}
}