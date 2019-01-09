#pragma once

#include "Connection.h"
#include "MessagePool.h"
#include "Performance.h"

namespace NetLib
{
	class IOCPServer
	{
	public:
		IOCPServer() {}
		~IOCPServer() {}

	public:
		virtual E_FUNCTION_RESULT StartServer(void);
		virtual void EndServer(void);

		bool ProcessNetworkMessages(OUT INT8& msgOperationType, OUT INT32& connectionIndex, char* pBuf, OUT INT16& copySize);
		void SendPacket(const INT32 connectionIndex, const void* pSendPacket, const INT16 packetSize);

		int GetMaxPacketSize(void) { return m_MaxPacketSize; }
		int GetMaxConnectionCount(void) { return m_MaxConnectionCount; }
		int GetPostMessagesThreadsCount(void) { return m_PostMessagesThreadsCount; }

	private:
		int GetServerInfo(const WCHAR* pKey);

		E_FUNCTION_RESULT CreateDirectories(void);
		bool CalculateWorkThreadCount(void);
		E_FUNCTION_RESULT LoadServerInfo(void);
		E_FUNCTION_RESULT CreateListenSocket(void);
		E_FUNCTION_RESULT CreateHandleIOCP(void);
		bool CreateMessageManager(void);
		bool LinkListenSocketIOCP(void);
		bool CreateConnections(void);
		bool CreatePerformance(void);
		bool CreateWorkThread(void);

		void WorkThread(void);

		E_FUNCTION_RESULT PostLogicMsg(Connection* pConnection, Message* pMsg, const DWORD packetSize = 0);

		void HandleExceptionWorkThread(const Connection* pConnection, const OVERLAPPED_EX* pOverlappedEx);
		void HandleExceptionCloseConnection(Connection* pConnection);

		void DoAccept(const OVERLAPPED_EX* pOverlappedEx);
		void DoRecv(OVERLAPPED_EX* pOverlappedEx, const DWORD ioSize);
		void DoSend(OVERLAPPED_EX* pOverlappedEx, const DWORD ioSize);

		void DoPostConnection(Connection* pConnection, const Message* pMsg, OUT INT8& msgOperationType, OUT INT32& connectionIndex);
		void DoPostClose(Connection* pConnection, const Message* pMsg, OUT INT8& msgOperationType, OUT INT32& connectionIndex);
		void DoPostRecvPacket(Connection* pConnection, const Message* pMsg, OUT INT8& msgOperationType, OUT INT32& connectionIndex, char* pBuf, OUT INT16& copySize, const DWORD ioSize);

	private:
		std::mutex m_MUTEX;
		SOCKET m_ListenSocket = INVALID_SOCKET;

		std::vector<std::unique_ptr<Connection>> m_Connections;

		unsigned short m_PortNumber = 0;
		int m_WorkThreadCount = INVALID_VALUE;
		int m_MaxRecvOverlappedBufferSize = INVALID_VALUE;
		int m_MaxSendOverlappedBufferSize = INVALID_VALUE;
		int m_MaxRecvConnectionBufferCount = INVALID_VALUE;
		int m_MaxSendConnectionBufferCount = INVALID_VALUE;
		int m_MaxPacketSize = INVALID_VALUE;
		int m_MaxConnectionCount = INVALID_VALUE;
		int m_MaxMessagePoolCount = INVALID_VALUE;
		int m_ExtraMessagePoolCount = INVALID_VALUE;
		int m_PerformancePacketMillisecondsTime = INVALID_VALUE;
		int m_PostMessagesThreadsCount = INVALID_VALUE;

		HANDLE m_hWorkIOCP = INVALID_HANDLE_VALUE;
		HANDLE m_hLogicIOCP = INVALID_HANDLE_VALUE;
		bool m_IsRunWorkThread = true;
		std::vector<std::unique_ptr<std::thread>> m_WorkThreads;

		std::unique_ptr<MessagePool> m_pMsgPool;

		std::unique_ptr<Performance> m_Performance;
	};
}