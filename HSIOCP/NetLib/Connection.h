#pragma once

#include "ConnectionInfo.h"

namespace NetLib
{
	class Connection
	{
	public:
		explicit Connection(const SOCKET listenSocket, const int index,
			const int recvRingBufferSize, const int sendRingBufferSize,
			const int recvOverlappedBufSize, const int sendOverlappedBufSize);
		~Connection();

	public:
		E_FUNCTION_RESULT CloseConnection(void);
		
		void DisconnectConnection(void);
		
		bool ResetConnection(void);
		
		bool BindIOCP(const HANDLE hWorkIOCP);
		
		E_FUNCTION_RESULT PostRecv(const char* pNextBuf, const DWORD remainByte);
		
		bool PostSend(const int sendSize);
		
		E_FUNCTION_RESULT PrepareSendPacket(OUT char** ppBuf, const int sendSize);


		ConnectionInfo& GetConnectionInfo(void) { return m_ConnectionInfo; }
		SOCKET GetClientSocket(void) { return m_ClientSocket; }

		void SetConnectionIP(const char* szIP) { CopyMemory(m_szIP, szIP, MAX_IP_LENGTH); }
		int GetConnectionIndex(void) { return m_Index; }

		int IncrementRecvIORefCount(void) { return InterlockedIncrement(reinterpret_cast<LPLONG>(&m_RecvIORefCount)); }
		int IncrementSendIORefCount(void) { return InterlockedIncrement(reinterpret_cast<LPLONG>(&m_SendIORefCount)); }
		int IncrementAcceptIORefCount(void) { return InterlockedIncrement(reinterpret_cast<LPLONG>(&m_AcceptIORefCount)); }
		int DecrementRecvIORefCount(void) { return (m_RecvIORefCount ? InterlockedDecrement(reinterpret_cast<LPLONG>(&m_RecvIORefCount)) : 0); }
		int DecrementSendIORefCount(void) { return (m_SendIORefCount ? InterlockedDecrement(reinterpret_cast<LPLONG>(&m_SendIORefCount)) : 0); }
		int DecrementAcceptIORefCount(void) { return (m_AcceptIORefCount ? InterlockedDecrement(reinterpret_cast<LPLONG>(&m_AcceptIORefCount)) : 0); }

	private:
		void Init(void);
		bool BindAcceptExSocket(void);

	private:
		SOCKET m_ClientSocket = INVALID_SOCKET;
		SOCKET m_ListenSocket = INVALID_SOCKET;

		std::mutex m_MUTEX;
		ConnectionInfo m_ConnectionInfo;

		int	m_RecvBufSize = INVALID_VALUE;
		int	m_SendBufSize = INVALID_VALUE;

		int m_Index = INVALID_VALUE;
		char m_szIP[MAX_IP_LENGTH] = { 0, };

		DWORD m_SendIORefCount = 0;
		DWORD m_RecvIORefCount = 0;
		DWORD m_AcceptIORefCount = 0;
	};
}