#include "stdafx.h"
#include "Connection.h"

#include "LogManager.h"

namespace NetLib
{
	Connection::Connection(const SOCKET listenSocket, const int index,
		const int recvRingBufferSize, const int sendRingBufferSize,
		const int recvOverlappedBufSize, const int sendOverlappedBufSize)
		: m_ListenSocket(listenSocket)
		, m_Index(index)
		, m_RecvBufSize(recvOverlappedBufSize)
		, m_SendBufSize(sendOverlappedBufSize)
	{
		Init();

		m_ConnectionInfo.pRecvOverlappedEx = new OVERLAPPED_EX(this);
		m_ConnectionInfo.pSendOverlappedEx = new OVERLAPPED_EX(this);

		m_ConnectionInfo.RingRecvBuffer.Create(recvRingBufferSize);
		m_ConnectionInfo.RingSendBuffer.Create(sendRingBufferSize);

		// TODO: BindAcceptExSocket 반환 값이 FALSE 일 때의 처리 추가
		BindAcceptExSocket();
	}

	Connection::~Connection()
	{
		m_ClientSocket = INVALID_SOCKET;
		m_ListenSocket = INVALID_SOCKET;
	}

	E_FUNCTION_RESULT Connection::CloseConnection(void)
	{
		//소켓만 종료한 채로 전부 처리될 때까지 대기
		if ((m_AcceptIORefCount != 0 || m_RecvIORefCount != 0 || m_SendIORefCount != 0) 
			&& m_ClientSocket != INVALID_SOCKET)
		{
			DisconnectConnection();
			return FUNCTION_RESULT_SUCCESS;
		}

		//한번만 접속 종료 처리를 하기 위해 사용
		if (InterlockedCompareExchange(reinterpret_cast<LPLONG>(&m_ConnectionInfo.IsClosed), TRUE, FALSE) == static_cast<long>(FALSE))
		{
			return FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG;
		}

		return FUNCTION_RESULT_SUCCESS;
	}

	void Connection::DisconnectConnection(void)
	{
		std::lock_guard<std::mutex> Lock(m_MUTEX);

		shutdown(m_ClientSocket, SD_BOTH);
		closesocket(m_ClientSocket);
		m_ClientSocket = INVALID_SOCKET;
	}

	bool Connection::ResetConnection(void)
	{
		std::lock_guard<std::mutex> Lock(m_MUTEX);

		if (m_ConnectionInfo.pRecvOverlappedEx != nullptr)
		{
			m_ConnectionInfo.pRecvOverlappedEx->OverlappedExRemainByte = 0;
			m_ConnectionInfo.pRecvOverlappedEx->OverlappedExTotalByte = 0;
		}

		if (m_ConnectionInfo.pSendOverlappedEx != nullptr)
		{
			m_ConnectionInfo.pSendOverlappedEx->OverlappedExRemainByte = 0;
			m_ConnectionInfo.pSendOverlappedEx->OverlappedExTotalByte = 0;
		}

		Init();
		return BindAcceptExSocket();
	}

	bool Connection::BindIOCP(const HANDLE hWorkIOCP)
	{
		std::lock_guard<std::mutex> Lock(m_MUTEX);

		//즉시 접속 종료하기 위한 소켓 옵션 추가
		linger li = { 0, 0 };
		li.l_onoff = 1;
		setsockopt(m_ClientSocket, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&li), sizeof(li));

		auto hIOCPHandle = CreateIoCompletionPort(
			reinterpret_cast<HANDLE>(m_ClientSocket),
			hWorkIOCP,
			reinterpret_cast<ULONG_PTR>(this),
			0);

		if (hIOCPHandle == INVALID_HANDLE_VALUE || hWorkIOCP != hIOCPHandle)
		{
			g_LogMgr.WriteLog(LOG_ERROR, "CreateIoCompletionPort() Fail({}) : {} {}", WSAGetLastError(), __FUNCTION__, __LINE__);
			return false;
		}

		return true;
	}

	E_FUNCTION_RESULT Connection::PostRecv(const char* pNextBuf, const DWORD remainByte)
	{
		// TODO: 함수 분리
		if (m_ConnectionInfo.IsConnect == FALSE || m_ConnectionInfo.pRecvOverlappedEx == nullptr)
		{
			g_LogMgr.WriteLog(LOG_ERROR, "IsConnect FALSE or pRecvOverlappedEx is NULL({}) : {} {}", m_ClientSocket, __FUNCTION__, __LINE__);

			if (CloseConnection() == FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG)
			{
				return FUNCTION_RESULT_FAIL_POST_CLOSE_MSG;
			}
			return FUNCTION_RESULT_FAIL;
		}

		m_ConnectionInfo.pRecvOverlappedEx->OverlappedExOperationType = OP_RECV;
		m_ConnectionInfo.pRecvOverlappedEx->OverlappedExRemainByte = remainByte;

		auto moveMark = static_cast<int>(remainByte - (m_ConnectionInfo.RingRecvBuffer.GetCurMark() - pNextBuf));
		m_ConnectionInfo.pRecvOverlappedEx->OverlappedExWsaBuf.len = m_RecvBufSize;
		m_ConnectionInfo.pRecvOverlappedEx->OverlappedExWsaBuf.buf = m_ConnectionInfo.RingRecvBuffer.ForwardMark(moveMark, m_RecvBufSize, remainByte);

		if (m_ConnectionInfo.pRecvOverlappedEx->OverlappedExWsaBuf.buf == nullptr)
		{
			g_LogMgr.WriteLog(LOG_ERROR, "RecvRingBuffer Full({}) : {} {}", m_ClientSocket, __FUNCTION__, __LINE__);

			if (CloseConnection() == FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG)
			{
				return FUNCTION_RESULT_FAIL_POST_CLOSE_MSG;
			}
			return FUNCTION_RESULT_FAIL;
		}

		m_ConnectionInfo.pRecvOverlappedEx->pOverlappedExSocketMessage = m_ConnectionInfo.pRecvOverlappedEx->OverlappedExWsaBuf.buf - remainByte;

		ZeroMemory(&m_ConnectionInfo.pRecvOverlappedEx->Overlapped, sizeof(OVERLAPPED));

		IncrementRecvIORefCount();

		DWORD flag = 0;
		DWORD recvByte = 0;
		auto result = WSARecv(
			m_ClientSocket,
			&m_ConnectionInfo.pRecvOverlappedEx->OverlappedExWsaBuf,
			1,
			&recvByte,
			&flag,
			&m_ConnectionInfo.pRecvOverlappedEx->Overlapped,
			NULL);

		if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			DecrementRecvIORefCount();

			g_LogMgr.WriteLog(LOG_ERROR, "WSARecv() Fail({}) : {} {}", WSAGetLastError(), __FUNCTION__, __LINE__);

			if (CloseConnection() == FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG)
			{
				return FUNCTION_RESULT_FAIL_POST_CLOSE_MSG;
			}
			return FUNCTION_RESULT_FAIL;
		}

		return FUNCTION_RESULT_SUCCESS;
	}

	bool Connection::PostSend(const int sendSize)
	{
		//남은 패킷이 존재하는지 확인하기 위한 과정
		if (sendSize > 0)
		{
			m_ConnectionInfo.RingSendBuffer.SetUsedBufferSize(sendSize);
		}

		if (InterlockedCompareExchange(reinterpret_cast<LPLONG>(&m_ConnectionInfo.IsSendable), FALSE, TRUE))
		{
			auto readSize = 0;
			char* pBuf = m_ConnectionInfo.RingSendBuffer.GetBuffer(m_SendBufSize, readSize);
			if (pBuf == nullptr)
			{
				InterlockedExchange(reinterpret_cast<LPLONG>(&m_ConnectionInfo.IsSendable), TRUE);
				return true;
			}

			ZeroMemory(&m_ConnectionInfo.pSendOverlappedEx->Overlapped, sizeof(OVERLAPPED));

			m_ConnectionInfo.pSendOverlappedEx->OverlappedExWsaBuf.len = readSize;
			m_ConnectionInfo.pSendOverlappedEx->OverlappedExWsaBuf.buf = pBuf;
			m_ConnectionInfo.pSendOverlappedEx->pOverlappedExConnection = this;

			m_ConnectionInfo.pSendOverlappedEx->OverlappedExRemainByte = 0;
			m_ConnectionInfo.pSendOverlappedEx->OverlappedExTotalByte = readSize;
			m_ConnectionInfo.pSendOverlappedEx->OverlappedExOperationType = OP_SEND;

			IncrementSendIORefCount();

			DWORD flag = 0;
			DWORD sendByte = 0;
			auto result = WSASend(
				m_ClientSocket,
				&m_ConnectionInfo.pSendOverlappedEx->OverlappedExWsaBuf,
				1,
				&sendByte,
				flag,
				&m_ConnectionInfo.pSendOverlappedEx->Overlapped,
				NULL);

			if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
			{
				DecrementSendIORefCount();

				g_LogMgr.WriteLog(LOG_ERROR, "WSASend() Fail({}) : {} {}", WSAGetLastError(), __FUNCTION__, __LINE__);

				if (CloseConnection() == FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG)
				{
					return false;
				}
			}
		}
		return true;
	}

	E_FUNCTION_RESULT Connection::PrepareSendPacket(OUT char** ppBuf, const int sendSize)
	{
		if (!m_ConnectionInfo.IsConnect)
		{
			*ppBuf = nullptr;
			return FUNCTION_RESULT_FAIL;
		}

		*ppBuf = m_ConnectionInfo.RingSendBuffer.ForwardMark(sendSize);
		if (*ppBuf == nullptr)
		{
			g_LogMgr.WriteLog(LOG_ERROR, "RingSendBuffer Full({}) : {} {}", m_ClientSocket, __FUNCTION__, __LINE__);

			if (CloseConnection() == FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG)
			{
				return FUNCTION_RESULT_FAIL_POST_CLOSE_MSG;
			}
			return FUNCTION_RESULT_FAIL;
		}

		return FUNCTION_RESULT_SUCCESS;
	}

	void Connection::Init(void)
	{
		ZeroMemory(m_szIP, MAX_IP_LENGTH);

		m_ConnectionInfo.Init();

		m_SendIORefCount = 0;
		m_RecvIORefCount = 0;
		m_AcceptIORefCount = 0;
	}

	bool Connection::BindAcceptExSocket(void)
	{
		ZeroMemory(&m_ConnectionInfo.pRecvOverlappedEx->Overlapped, sizeof(OVERLAPPED));

		m_ConnectionInfo.pRecvOverlappedEx->OverlappedExWsaBuf.buf = m_ConnectionInfo.AddrBuf;
		m_ConnectionInfo.pRecvOverlappedEx->pOverlappedExSocketMessage = m_ConnectionInfo.pRecvOverlappedEx->OverlappedExWsaBuf.buf;
		m_ConnectionInfo.pRecvOverlappedEx->OverlappedExWsaBuf.len = m_RecvBufSize;
		m_ConnectionInfo.pRecvOverlappedEx->OverlappedExOperationType = OP_ACCEPT;
		m_ConnectionInfo.pRecvOverlappedEx->pOverlappedExConnection = this;

		m_ClientSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (m_ClientSocket == INVALID_SOCKET)
		{
			g_LogMgr.WriteLog(LOG_ERROR, "Socket Connection Fail({}) : {} {}", WSAGetLastError(), __FUNCTION__, __LINE__);
			return false;
		}

		IncrementAcceptIORefCount();

		DWORD acceptByte = 0;
		auto result = AcceptEx(
			m_ListenSocket,
			m_ClientSocket,
			m_ConnectionInfo.pRecvOverlappedEx->OverlappedExWsaBuf.buf,
			0,
			sizeof(SOCKADDR_IN) + 16,
			sizeof(SOCKADDR_IN) + 16,
			&acceptByte,
			reinterpret_cast<LPOVERLAPPED>(m_ConnectionInfo.pRecvOverlappedEx));

		if (!result && WSAGetLastError() != WSA_IO_PENDING)
		{
			DecrementAcceptIORefCount();

			g_LogMgr.WriteLog(LOG_ERROR, "AcceptEx() Fail({}) : {} {}", WSAGetLastError(), __FUNCTION__, __LINE__);
			return false;
		}

		return true;
	}
}