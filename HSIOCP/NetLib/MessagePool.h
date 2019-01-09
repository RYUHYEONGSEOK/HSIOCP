#pragma once

namespace NetLib
{
	class Connection;
	class MessagePool
	{
	public:
		explicit MessagePool(const int maxMsgPoolCount, const int extraMsgPoolCount);
		~MessagePool();

	public:
		bool CheckCreate(void);
		bool DeallocMsg(Message* pMsg);
		Message* AllocMsg(void);

	private:
		bool CreateMessagePool(void);
		void DestroyMessagePool(void);

	private:
		concurrency::concurrent_queue<Message*> m_MessagePool;
		std::list<Message*> m_OriginalMessages; //TODO: 최흥배. 이것은 필요가 없는 것 같습니다.

		int m_MaxMessagePoolCount = INVALID_VALUE;
		int m_ExtraMessagePoolCount = INVALID_VALUE;
	};
}