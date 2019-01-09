#include "stdafx.h"
#include "IOCPServer.h"

#include "LogManager.h"
#include "MiniDump.h"

namespace NetLib
{
	E_FUNCTION_RESULT IOCPServer::StartServer(void)
	{
		MiniDump::Begin();

		E_FUNCTION_RESULT result = CreateDirectories();
		if (result != FUNCTION_RESULT_SUCCESS)
		{
			return result;
		}

		if (!CalculateWorkThreadCount())
		{
			return FUNCTION_RESULT_FAIL_WORKTHREAD_INFO;
		}

		result = LoadServerInfo();
		if (result != FUNCTION_RESULT_SUCCESS)
		{
			return result;
		}

		result = CreateListenSocket();
		if (result != FUNCTION_RESULT_SUCCESS)
		{
			return result;
		}

		result = CreateHandleIOCP();
		if (result != FUNCTION_RESULT_SUCCESS)
		{
			return result;
		}

		if (!CreateMessageManager())
		{
			return FUNCTION_RESULT_FAIL_CREATE_MESSAGE_MANAGER;
		}

		if (!LinkListenSocketIOCP())
		{
			return FUNCTION_RESULT_FAIL_LINK_IOCP;
		}

		if (!CreateConnections())
		{
			return FUNCTION_RESULT_FAIL_CREATE_CONNECTIONS;
		}

		if (!CreatePerformance())
		{
			return FUNCTION_RESULT_FAIL_CREATE_PERFORMANCE;
		}

		if (!CreateWorkThread())
		{
			return FUNCTION_RESULT_FAIL_CREATE_WORKTHREAD;
		}

		g_LogMgr.WriteLog(LOG_DEBUG, "Start Server Completion");
		return FUNCTION_RESULT_SUCCESS;
	}

	void IOCPServer::EndServer(void)
	{
		if (m_hWorkIOCP != INVALID_HANDLE_VALUE)
		{
			m_IsRunWorkThread = false;

			// joinable 상태라면 스레드 살아 있는 상태이므로 한번 PostQueuedCompletionStatus 보내고
			// 모든 스레드가  joinable()이 false라면 스레드가 다 종료된 상태이므로 더 이상 스레드 살아 있는지 알아보지 않아도 됨?
			for (int i = 0; i < m_WorkThreadCount; ++i)
			{
				PostQueuedCompletionStatus(m_hWorkIOCP, 0, 0, nullptr);
			}
						
			for (int i = 0; i < m_WorkThreads.size(); ++i)
			{
				if (m_WorkThreads[i].get()->joinable())
				{
					m_WorkThreads[i].get()->join();
					continue;
				}

				while (true)
				{
					PostQueuedCompletionStatus(m_hWorkIOCP, 0, 0, nullptr);
					Sleep(MAX_THREAD_JOIN_SLEEP_TIME);

					if (m_WorkThreads[i].get()->joinable())
					{
						m_WorkThreads[i].get()->join();
						break;
					}
				}
			}

			CloseHandle(m_hWorkIOCP);
		}

		if (m_hLogicIOCP != INVALID_HANDLE_VALUE)
		{
			PostQueuedCompletionStatus(m_hLogicIOCP, 0, 0, nullptr);

			CloseHandle(m_hLogicIOCP);
		}

		if (m_ListenSocket != INVALID_SOCKET)
		{
			closesocket(m_ListenSocket);
			m_ListenSocket = INVALID_SOCKET;
		}

		WSACleanup();

		g_LogMgr.WriteLog(LOG_DEBUG, "End Server Completion");

		MiniDump::End();
	}

	bool IOCPServer::ProcessNetworkMessages(OUT INT8& msgOperationType, OUT INT32& connectionIndex, char* pBuf, OUT INT16& copySize)
	{
		Message* pMsg = nullptr;
		Connection* pConnection = nullptr;
		DWORD ioSize = 0;

		auto result = GetQueuedCompletionStatus(
			m_hLogicIOCP,
			&ioSize,
			reinterpret_cast<PULONG_PTR>(&pConnection),
			reinterpret_cast<LPOVERLAPPED*>(&pMsg),
			INFINITE);

		if (result && pConnection == nullptr)
		{
			return false;
		}

		if (pMsg == nullptr)
		{
			return true;
		}

		switch (pMsg->OperationType)
		{
		case OP_CONNECTION:
			DoPostConnection(pConnection, pMsg, msgOperationType, connectionIndex);
			break;
		case OP_CLOSE:
			DoPostClose(pConnection, pMsg, msgOperationType, connectionIndex);
			break;
		case OP_RECV_PACKET:
			DoPostRecvPacket(pConnection, pMsg, msgOperationType, connectionIndex, pBuf, copySize, ioSize);
			break;
		}

		m_pMsgPool.get()->DeallocMsg(pMsg);
		return true;
	}

	void IOCPServer::SendPacket(const INT32 connectionIndex, const void* pSendPacket, const INT16 packetSize)
	{
		if (connectionIndex < 0 || connectionIndex >= m_MaxConnectionCount)
		{
			return;
		}

		Connection* pConnection = m_Connections[connectionIndex].get();
		char* pSendRingBuf = nullptr;
		auto result = pConnection->PrepareSendPacket(&pSendRingBuf, packetSize);
		if (result == FUNCTION_RESULT_FAIL)
		{
			return;
		}
		else if (result == FUNCTION_RESULT_FAIL_POST_CLOSE_MSG)
		{
			HandleExceptionCloseConnection(pConnection);
			return;
		}

		CopyMemory(pSendRingBuf, pSendPacket, packetSize);

		if (!pConnection->PostSend(packetSize))
		{
			HandleExceptionCloseConnection(pConnection);
		}
	}

	int IOCPServer::GetServerInfo(const WCHAR* pKey)
	{
		WCHAR buf[MAX_INFO_BUF_LENGTH] = { 0, };
		auto result = GetPrivateProfileString(SERVER_INFO_SECTION, pKey, L"-1", buf, MAX_INFO_BUF_LENGTH, SERVER_INFO_PATH);
		std::wstring ResultString(buf, result);

		return std::stoi(ResultString);
	}

	E_FUNCTION_RESULT IOCPServer::CreateDirectories(void)
	{
		auto result = _access_s(LOGGER_CHECK_PATH, 0);
		if (result != 0)
		{
			auto isSuccess = CreateDirectoryA(LOGGER_CHECK_PATH, nullptr);
			if (isSuccess == FALSE)
			{
				return FUNCTION_RESULT_FAIL_MAKE_DIRECTORIES_LOG;
			}
		}

		result = _waccess_s(DUMP_CHECK_PATH, 0);
		if (result != 0)
		{
			auto isSuccess = CreateDirectoryW(DUMP_CHECK_PATH, nullptr);
			if (isSuccess == FALSE)
			{
				return FUNCTION_RESULT_FAIL_MAKE_DIRECTORIES_DUMP;
			}
		}

		return FUNCTION_RESULT_SUCCESS;
	}

	bool IOCPServer::CalculateWorkThreadCount(void)
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);

		m_WorkThreadCount = systemInfo.dwNumberOfProcessors * 2;

		g_LogMgr.WriteLog(LOG_INFO, "WORK_THREAD_COUNT: {}", m_WorkThreadCount);
		return true;
	}

	E_FUNCTION_RESULT IOCPServer::LoadServerInfo(void)
	{
		auto outPutValue = GetServerInfo(PORT);
		if (outPutValue == INVALID_VALUE)
		{
			return FUNCTION_RESULT_FAIL_SERVER_INFO_PORT;
		}
		m_PortNumber = static_cast<unsigned short>(outPutValue);
		g_LogMgr.WriteLog(LOG_INFO, "PORT: {}", outPutValue);

		outPutValue = GetServerInfo(MAX_RECV_OVELAPPED_BUFFER_SIZE);
		if (outPutValue == INVALID_VALUE)
		{
			return FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_RECV_OVELAPPED_BUFFER_SIZE;
		}
		m_MaxRecvOverlappedBufferSize = outPutValue;
		g_LogMgr.WriteLog(LOG_INFO, "MAX_RECV_OVELAPPED_BUFFER_SIZE: {}", outPutValue);

		outPutValue = GetServerInfo(MAX_SEND_OVELAPPED_BUFFER_SIZE);
		if (outPutValue == INVALID_VALUE)
		{
			return FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_SEND_OVELAPPED_BUFFER_SIZE;
		}
		m_MaxSendOverlappedBufferSize = outPutValue;
		g_LogMgr.WriteLog(LOG_INFO, "MAX_SEND_OVELAPPED_BUFFER_SIZE: {}", outPutValue);

		outPutValue = GetServerInfo(MAX_RECV_CONNECTION_BUFFER_SIZE);
		if (outPutValue == INVALID_VALUE)
		{
			return FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_RECV_CONNECTION_BUFFER_SIZE;
		}
		m_MaxRecvConnectionBufferCount = outPutValue;
		g_LogMgr.WriteLog(LOG_INFO, "MAX_RECV_CONNECTION_BUFFER_SIZE: {}", outPutValue);

		outPutValue = GetServerInfo(MAX_SEND_CONNECTION_BUFFER_SIZE);
		if (outPutValue == INVALID_VALUE)
		{
			return FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_SEND_CONNECTION_BUFFER_SIZE;
		}
		m_MaxSendConnectionBufferCount = outPutValue;
		g_LogMgr.WriteLog(LOG_INFO, "MAX_SEND_CONNECTION_BUFFER_SIZE: {}", outPutValue);

		outPutValue = GetServerInfo(MAX_PACKET_SIZE);
		if (outPutValue == INVALID_VALUE)
		{
			return FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_PACKET_SIZE;
		}
		m_MaxPacketSize = outPutValue;
		g_LogMgr.WriteLog(LOG_INFO, "MAX_PACKET_SIZE: {}", outPutValue);

		outPutValue = GetServerInfo(MAX_CONNECTION_COUNT);
		if (outPutValue == INVALID_VALUE)
		{
			return FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_CONNECTION_COUNT;
		}
		m_MaxConnectionCount = outPutValue;
		g_LogMgr.WriteLog(LOG_INFO, "MAX_CONNECTION_COUNT: {}", outPutValue);

		outPutValue = GetServerInfo(MAX_MESSAGE_POOL_COUNT);
		if (outPutValue == INVALID_VALUE)
		{
			return FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_MESSAGE_POOL_COUNT;
		}
		m_MaxMessagePoolCount = outPutValue;
		g_LogMgr.WriteLog(LOG_INFO, "MAX_MESSAGE_POOL_COUNT: {}", outPutValue);

		outPutValue = GetServerInfo(EXTRA_MESSAGE_POOL_COUNT);
		if (outPutValue == INVALID_VALUE)
		{
			return FUNCTION_RESULT_FAIL_SERVER_INFO_EXTRA_MESSAGE_POOL_COUNT;
		}
		m_ExtraMessagePoolCount = outPutValue;
		g_LogMgr.WriteLog(LOG_INFO, "EXTRA_MESSAGE_POOL_COUNT: {}", outPutValue);

		outPutValue = GetServerInfo(PERFORMANCE_PACKET_MILLISECONDS_TIME);
		if (outPutValue == INVALID_VALUE)
		{
			return FUNCTION_RESULT_FAIL_SERVER_INFO_PERFORMANCE_PACKET_MILLISECONDS_TIME;
		}
		m_PerformancePacketMillisecondsTime = outPutValue;
		g_LogMgr.WriteLog(LOG_INFO, "PERFORMANCE_PACKET_MILLISECONDS_TIME: {}", outPutValue);

		outPutValue = GetServerInfo(POST_MESSAGES_THREADS_COUNT);
		if (outPutValue == INVALID_VALUE)
		{
			return FUNCTION_RESULT_FAIL_SERVER_INFO_POST_MESSAGES_THREADS_COUNT;
		}
		m_PostMessagesThreadsCount = outPutValue;
		g_LogMgr.WriteLog(LOG_INFO, "POST_MESSAGES_THREADS_COUNT: {}", outPutValue);

		return FUNCTION_RESULT_SUCCESS;
	}

	E_FUNCTION_RESULT IOCPServer::CreateListenSocket(void)
	{
		WSADATA wsaData;
		auto result = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (result != 0)
		{
			return FUNCTION_RESULT_FAIL_CREATE_LISTENSOCKET_STARTUP;
		}

		m_ListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (m_ListenSocket == INVALID_SOCKET)
		{
			return FUNCTION_RESULT_FAIL_CREATE_LISTENSOCKET_SOCKET;
		}

		SOCKADDR_IN	addr;
		ZeroMemory(&addr, sizeof(SOCKADDR_IN));

		addr.sin_family = AF_INET;
		addr.sin_port = htons(m_PortNumber);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (::bind(m_ListenSocket, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR)
		{
			return FUNCTION_RESULT_FAIL_CREATE_LISTENSOCKET_BIND;
		}

		if (::listen(m_ListenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			return FUNCTION_RESULT_FAIL_CREATE_LISTENSOCKET_LISTEN;
		}

		return FUNCTION_RESULT_SUCCESS;
	}

	E_FUNCTION_RESULT IOCPServer::CreateHandleIOCP(void)
	{
		m_hWorkIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, m_WorkThreadCount);
		if (m_hWorkIOCP == INVALID_HANDLE_VALUE)
		{
			return FUNCTION_RESULT_FAIL_HANDLEIOCP_WORK;
		}

		m_hLogicIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
		if (m_hLogicIOCP == INVALID_HANDLE_VALUE)
		{
			return FUNCTION_RESULT_FAIL_HANDLEIOCP_LOGIC;
		}

		return FUNCTION_RESULT_SUCCESS;
	}

	bool IOCPServer::CreateMessageManager(void)
	{
		m_pMsgPool = std::make_unique<MessagePool>(m_MaxMessagePoolCount, m_ExtraMessagePoolCount);
		if (!m_pMsgPool.get()->CheckCreate())
		{
			return false;
		}

		return true;
	}

	bool IOCPServer::LinkListenSocketIOCP(void)
	{
		auto hIOCPHandle = CreateIoCompletionPort(
			reinterpret_cast<HANDLE>(m_ListenSocket),
			m_hWorkIOCP,
			0,
			0);

		if (hIOCPHandle == INVALID_HANDLE_VALUE || m_hWorkIOCP != hIOCPHandle)
		{
			return false;
		}

		return true;
	}

	bool IOCPServer::CreateConnections(void)
	{
		for (int i = 0; i < m_MaxConnectionCount; ++i)
		{
			m_Connections.push_back(std::make_unique<Connection>(m_ListenSocket, i,
				m_MaxRecvConnectionBufferCount, m_MaxRecvConnectionBufferCount,
				m_MaxRecvOverlappedBufferSize, m_MaxSendOverlappedBufferSize));
		}

		return true;
	}

	bool IOCPServer::CreatePerformance(void)
	{
		if (m_PerformancePacketMillisecondsTime == INVALID_VALUE)
		{
			return false;
		}

		m_Performance = std::make_unique<Performance>(m_PerformancePacketMillisecondsTime);

		return true;
	}

	bool IOCPServer::CreateWorkThread(void)
	{
		if (m_WorkThreadCount == INVALID_VALUE)
		{
			return false;
		}

		for (int i = 0; i < m_WorkThreadCount; ++i)
		{
			m_WorkThreads.push_back(std::make_unique<std::thread>([&]() {WorkThread(); }));
		}

		return true;
	}

	void IOCPServer::WorkThread(void)
	{
		while (m_IsRunWorkThread)
		{
			DWORD ioSize = 0;
			OVERLAPPED_EX* pOverlappedEx = nullptr;
			Connection* pConnection = nullptr;

			// TODO: 모든 GetQueuedCompletionStatus를 GetQueuedCompletionStatusEx 버전을 사용하도록 변경
			auto result = GetQueuedCompletionStatus(
				m_hWorkIOCP,
				&ioSize,
				reinterpret_cast<PULONG_PTR>(&pConnection),
				reinterpret_cast<LPOVERLAPPED*>(&pOverlappedEx),
				INFINITE);

			if (pOverlappedEx == nullptr)
			{
				if (WSAGetLastError() != 0 && WSAGetLastError() != WSA_IO_PENDING)
				{
					g_LogMgr.WriteLog(LOG_ERROR, "GetQueuedCompletionStatus() Fail({}) : {} {}", WSAGetLastError(), __FUNCTION__, __LINE__);
				}
				continue;
			}

			if (!result || (0 == ioSize && OP_ACCEPT != pOverlappedEx->OverlappedExOperationType))
			{
				HandleExceptionWorkThread(pConnection, pOverlappedEx);
				continue;
			}

			switch (pOverlappedEx->OverlappedExOperationType)
			{
			case OP_ACCEPT:
				DoAccept(pOverlappedEx);
				break;
			case OP_RECV:
				DoRecv(pOverlappedEx, ioSize);
				break;
			case OP_SEND:
				DoSend(pOverlappedEx, ioSize);
				break;
			}
		}
	}

	E_FUNCTION_RESULT IOCPServer::PostLogicMsg(Connection* pConnection, Message* pMsg, const DWORD packetSize/* = 0*/)
	{
		if (m_hLogicIOCP == INVALID_HANDLE_VALUE || pMsg == nullptr)
		{
			return FUNCTION_RESULT_FAIL_MESSAGE_NULL;
		}

		auto result = PostQueuedCompletionStatus(
			m_hLogicIOCP,
			packetSize,
			reinterpret_cast<ULONG_PTR>(pConnection),
			reinterpret_cast<LPOVERLAPPED>(pMsg));

		if (!result)
		{
			g_LogMgr.WriteLog(LOG_ERROR, "PostQueuedCompletionStatus() Fail({}) : {} {}", GetLastError(), __FUNCTION__, __LINE__);
			return FUNCTION_RESULT_FAIL_PQCS;
		}
		return FUNCTION_RESULT_SUCCESS;
	}

	void IOCPServer::HandleExceptionWorkThread(const Connection* pConnection, const OVERLAPPED_EX* pOverlappedEx)
	{
		if (pOverlappedEx == nullptr && pConnection == nullptr)
		{
			return;
		}

		if (pOverlappedEx->pOverlappedExConnection == nullptr)
		{
			return;
		}

		//Connection 접속 종료 시 남은 IO 처리
		switch (pOverlappedEx->OverlappedExOperationType)
		{
		case OP_ACCEPT:
			pOverlappedEx->pOverlappedExConnection->DecrementAcceptIORefCount();
			break;
		case OP_RECV:
			pOverlappedEx->pOverlappedExConnection->DecrementRecvIORefCount();
			break;
		case OP_SEND:
			pOverlappedEx->pOverlappedExConnection->DecrementSendIORefCount();
			break;
		}

		if (pOverlappedEx->pOverlappedExConnection->CloseConnection() == FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG)
		{
			HandleExceptionCloseConnection(pOverlappedEx->pOverlappedExConnection);
		}

		return;
	}

	void IOCPServer::HandleExceptionCloseConnection(Connection* pConnection)
	{
		if (pConnection == nullptr)
		{
			g_LogMgr.WriteLog(LOG_CRITICAL, "pConnection is nullptr : {} {}", __FUNCTION__, __LINE__);
			return;
		}

		InterlockedExchange(reinterpret_cast<LPLONG>(&pConnection->GetConnectionInfo().IsConnect), FALSE);

		pConnection->DisconnectConnection();

		// TODO: OP_CLOSE와 OP_CONNECTION 등 세션별로 1개씩 사용하고, 꼭 있어야 하는 것은 세션별로 가지고 있고 사용
		// 아래처럼 Message가 부족하게 되서 예외처러하게 되면 뒤에 파악하기 어려울 것 예상
		Message* pMsg = m_pMsgPool.get()->AllocMsg();
		if (pMsg == nullptr)
		{
			pConnection->ResetConnection();
			return;
		}

		pMsg->SetMessage(OP_CLOSE, nullptr);
		if (PostLogicMsg(pConnection, pMsg) != FUNCTION_RESULT_SUCCESS)
		{
			pConnection->ResetConnection();
			m_pMsgPool.get()->DeallocMsg(pMsg);
		}
	}

	void IOCPServer::DoAccept(const OVERLAPPED_EX* pOverlappedEx)
	{
		Connection* pConnection = pOverlappedEx->pOverlappedExConnection;
		if (pConnection == nullptr)
		{
			return;
		}

		pConnection->DecrementAcceptIORefCount();

		SOCKADDR* pLocalSockAddr = nullptr;
		SOCKADDR* pRemoteSockAddr = nullptr;

		int	localSockaddrLen = 0;
		int	remoteSockaddrLen = 0;

		GetAcceptExSockaddrs(pConnection->GetConnectionInfo().AddrBuf, 0,
			sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
			&pLocalSockAddr, &localSockaddrLen,
			&pRemoteSockAddr, &remoteSockaddrLen);

		if (remoteSockaddrLen != 0)
		{
			SOCKADDR_IN* pRemoteSockAddrIn = reinterpret_cast<SOCKADDR_IN*>(pRemoteSockAddr);
			if (pRemoteSockAddrIn != nullptr)
			{
				char szIP[MAX_IP_LENGTH] = { 0, };
				inet_ntop(AF_INET, &pRemoteSockAddrIn->sin_addr, szIP, sizeof(szIP));

				pConnection->SetConnectionIP(szIP);
			}
		}
		else
		{
			g_LogMgr.WriteLog(LOG_ERROR, "GetAcceptExSockaddrs() Fail({}) : {} {}", WSAGetLastError(), __FUNCTION__, __LINE__);

			if (pConnection->CloseConnection() == FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG)
			{
				HandleExceptionCloseConnection(pConnection);
			}
			return;
		}

		if (!pConnection->BindIOCP(m_hWorkIOCP))
		{
			if (pConnection->CloseConnection() == FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG)
			{
				HandleExceptionCloseConnection(pConnection);
			}
			return;
		}

		InterlockedExchange(reinterpret_cast<LPLONG>(&pConnection->GetConnectionInfo().IsConnect), TRUE);

		auto result = pConnection->PostRecv(pConnection->GetConnectionInfo().RingRecvBuffer.GetBeginMark(), 0);
		if (result == FUNCTION_RESULT_FAIL)
		{
			return;
		}
		else if (result == FUNCTION_RESULT_FAIL_POST_CLOSE_MSG)
		{
			g_LogMgr.WriteLog(LOG_ERROR, "PostRecv() Fail({}) : {} {}", WSAGetLastError(), __FUNCTION__, __LINE__);

			HandleExceptionCloseConnection(pConnection);
			return;
		}

		Message* pMsg = m_pMsgPool.get()->AllocMsg();
		if (pMsg == nullptr)
		{
			pConnection->DisconnectConnection();
			pConnection->ResetConnection();
			return;
		}

		pMsg->SetMessage(OP_CONNECTION, nullptr);
		if (PostLogicMsg(pConnection, pMsg) != FUNCTION_RESULT_SUCCESS)
		{
			pConnection->DisconnectConnection();
			pConnection->ResetConnection();

			m_pMsgPool.get()->DeallocMsg(pMsg);
			return;
		}
	}

	void IOCPServer::DoRecv(OVERLAPPED_EX* pOverlappedEx, const DWORD ioSize)
	{		
		Connection* pConnection = pOverlappedEx->pOverlappedExConnection;
		if (pConnection == nullptr)
		{
			return;
		}

		pConnection->DecrementRecvIORefCount();

		short packetSize = 0;
		int remainByte = pOverlappedEx->OverlappedExRemainByte;
		char* pCurrent = nullptr;
		char* pNext = nullptr;

		pOverlappedEx->OverlappedExWsaBuf.buf = pOverlappedEx->pOverlappedExSocketMessage;
		pOverlappedEx->OverlappedExRemainByte += ioSize;

		if (PACKET_SIZE_LENGTH <= pOverlappedEx->OverlappedExRemainByte)
		{
			CopyMemory(&packetSize, pOverlappedEx->OverlappedExWsaBuf.buf, PACKET_SIZE_LENGTH);
		}
		else
		{
			packetSize = 0;
		}

		if (packetSize <= 0 || packetSize > pConnection->GetConnectionInfo().RingRecvBuffer.GetBufferSize())
		{
			g_LogMgr.WriteLog(LOG_CRITICAL, "Arrived Wrong Packet : {} {}", __FUNCTION__, __LINE__);

			if (pConnection->CloseConnection() == FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG)
			{
				HandleExceptionCloseConnection(pConnection);
			}
			return;
		}

		pOverlappedEx->OverlappedExTotalByte = packetSize;

		//아직 데이터를 받지 못한 상황
		if ((pOverlappedEx->OverlappedExRemainByte < static_cast<DWORD>(packetSize)))
		{
			remainByte = pOverlappedEx->OverlappedExRemainByte;
			pNext = pOverlappedEx->OverlappedExWsaBuf.buf;
		}
		//하나 이상의 데이터를 다 받은 상황
		else
		{
			pCurrent = pOverlappedEx->OverlappedExWsaBuf.buf;
			auto currentSize = packetSize;

			remainByte = pOverlappedEx->OverlappedExRemainByte;

			Message* pMsg = m_pMsgPool.get()->AllocMsg();
			if (pMsg == nullptr)
			{
				return;
			}

			pMsg->SetMessage(OP_RECV_PACKET, pCurrent);
			if (PostLogicMsg(pConnection, pMsg, currentSize) != FUNCTION_RESULT_SUCCESS)
			{
				m_pMsgPool.get()->DeallocMsg(pMsg);
				return;
			}

			remainByte -= currentSize;
			pNext = pCurrent + currentSize;

			while (true)
			{
				if (remainByte >= PACKET_SIZE_LENGTH)
				{
					CopyMemory(&packetSize, pNext, PACKET_SIZE_LENGTH);
					currentSize = packetSize;

					if (0 >= packetSize || packetSize > pConnection->GetConnectionInfo().RingRecvBuffer.GetBufferSize())
					{
						g_LogMgr.WriteLog(LOG_CRITICAL, "Arrived Wrong Packet : {} {}", __FUNCTION__, __LINE__);

						if (pConnection->CloseConnection() == FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG)
						{
							HandleExceptionCloseConnection(pConnection);
						}
						return;
					}
					pOverlappedEx->OverlappedExTotalByte = currentSize;
					if (remainByte >= currentSize)
					{
						pMsg = m_pMsgPool.get()->AllocMsg();
						if (pMsg == nullptr)
						{
							return;
						}

						pMsg->SetMessage(OP_RECV_PACKET, pNext);
						if (PostLogicMsg(pConnection, pMsg, currentSize) != FUNCTION_RESULT_SUCCESS)
						{
							m_pMsgPool.get()->DeallocMsg(pMsg);
							return;
						}

						remainByte -= currentSize;
						pNext += currentSize;
					}
					else
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
		}

		if (pConnection->PostRecv(pNext, remainByte) == FUNCTION_RESULT_FAIL_POST_CLOSE_MSG)
		{
			HandleExceptionCloseConnection(pConnection);
		}
	}

	void IOCPServer::DoSend(OVERLAPPED_EX* pOverlappedEx, const DWORD ioSize)
	{
		Connection* pConnection = pOverlappedEx->pOverlappedExConnection;
		if (pConnection == nullptr)
		{
			return;
		}

		pConnection->DecrementSendIORefCount();

		pOverlappedEx->OverlappedExRemainByte += ioSize;

		//모든 메세지 전송하지 못한 상황
		if (static_cast<DWORD>(pOverlappedEx->OverlappedExTotalByte) > pOverlappedEx->OverlappedExRemainByte)
		{
			pConnection->IncrementSendIORefCount();

			pOverlappedEx->OverlappedExWsaBuf.buf += ioSize;
			pOverlappedEx->OverlappedExWsaBuf.len -= ioSize;

			ZeroMemory(&pOverlappedEx->Overlapped, sizeof(OVERLAPPED));

			DWORD flag = 0;
			DWORD sendByte = 0;
			auto result = WSASend(
				pConnection->GetClientSocket(),
				&(pOverlappedEx->OverlappedExWsaBuf),
				1,
				&sendByte,
				flag,
				&(pOverlappedEx->Overlapped),
				NULL);

			if (result == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
			{
				pConnection->DecrementSendIORefCount();

				g_LogMgr.WriteLog(LOG_ERROR, "WSASend() Fail({}) : {} {}", WSAGetLastError(), __FUNCTION__, __LINE__);

				if (pConnection->CloseConnection() == FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG)
				{
					HandleExceptionCloseConnection(pConnection);
				}
				return;
			}
		}
		//모든 메세지 전송한 상황
		else
		{
			pConnection->GetConnectionInfo().RingSendBuffer.ReleaseBuffer(pOverlappedEx->OverlappedExTotalByte);

			InterlockedExchange(reinterpret_cast<LPLONG>(&pConnection->GetConnectionInfo().IsSendable), TRUE);

			if (!pConnection->PostSend(0))
			{
				HandleExceptionCloseConnection(pConnection);
			}
		}
	}

	void IOCPServer::DoPostConnection(Connection* pConnection, const Message* pMsg, OUT INT8& msgOperationType, OUT INT32& connectionIndex)
	{
		if (!pConnection->GetConnectionInfo().IsConnect)
		{
			return;
		}

		msgOperationType = pMsg->OperationType;
		connectionIndex = pConnection->GetConnectionIndex();

		g_LogMgr.WriteLog(LOG_DEBUG, "Connect Connection: {}", pConnection->GetConnectionIndex());
	}

	void IOCPServer::DoPostClose(Connection* pConnection, const Message* pMsg, OUT INT8& msgOperationType, OUT INT32& connectionIndex)
	{
		msgOperationType = pMsg->OperationType;
		connectionIndex = pConnection->GetConnectionIndex();

		g_LogMgr.WriteLog(LOG_DEBUG, "Disconnect Connection: {}", pConnection->GetConnectionIndex());

		pConnection->ResetConnection();
	}

	void IOCPServer::DoPostRecvPacket(Connection* pConnection, const Message* pMsg, OUT INT8& msgOperationType, OUT INT32& connectionIndex, char* pBuf, OUT INT16& copySize, const DWORD ioSize)
	{
		if (pMsg->pContents == nullptr)
		{
			return;
		}

		msgOperationType = pMsg->OperationType;
		connectionIndex = pConnection->GetConnectionIndex();
		CopyMemory(pBuf, pMsg->pContents, ioSize);

		copySize = static_cast<INT16>(ioSize);

		pConnection->GetConnectionInfo().RingRecvBuffer.ReleaseBuffer(ioSize);

		m_Performance.get()->IncrementPacketProcessCount();
	}
}