#pragma once

#define SAFE_DELETE(x) if( x != nullptr ) { delete x; x = nullptr; } 
#define SAFE_DELETE_ARR(x) if( x != nullptr ) { delete [] x; x = nullptr;}

namespace NetLib
{
	const int MAX_IP_LENGTH = 20;
	const int MAX_ADDR_LENGTH = 1024;
	const int MAX_INFO_BUF_LENGTH = 128;
	const int MAX_LOG_LENGTH = 256;
	const int INVALID_VALUE = -1;

	const int MAX_THREAD_JOIN_SLEEP_TIME = 50;

	const int PACKET_SIZE_LENGTH = 2;
	const int PACKET_TYPE_LENGTH = 2;

	const WCHAR SERVER_INFO_PATH[] = L"..\\Bin\\Data\\ServerInfo.ini";

	const WCHAR SERVER_INFO_SECTION[] = L"ServerInfo";
	const WCHAR PORT[] = L"PORT";
	const WCHAR MAX_RECV_OVELAPPED_BUFFER_SIZE[] = L"MAX_RECV_OVELAPPED_BUFFER_SIZE";
	const WCHAR MAX_SEND_OVELAPPED_BUFFER_SIZE[] = L"MAX_SEND_OVELAPPED_BUFFER_SIZE";
	const WCHAR MAX_RECV_CONNECTION_BUFFER_SIZE[] = L"MAX_RECV_CONNECTION_BUFFER_SIZE";
	const WCHAR MAX_SEND_CONNECTION_BUFFER_SIZE[] = L"MAX_SEND_CONNECTION_BUFFER_SIZE";
	const WCHAR MAX_PACKET_SIZE[] = L"MAX_PACKET_SIZE";
	const WCHAR MAX_CONNECTION_COUNT[] = L"MAX_CONNECTION_COUNT";
	const WCHAR MAX_MESSAGE_POOL_COUNT[] = L"MAX_MESSAGE_POOL_COUNT";
	const WCHAR EXTRA_MESSAGE_POOL_COUNT[] = L"EXTRA_MESSAGE_POOL_COUNT";
	const WCHAR PERFORMANCE_PACKET_MILLISECONDS_TIME[] = L"PERFORMANCE_PACKET_MILLISECONDS_TIME";
	const WCHAR POST_MESSAGES_THREADS_COUNT[] = L"POST_MESSAGES_THREADS_COUNT";

	const char LOGGER_NAME[] = "HSIOCP";
	const char LOGGER_CHECK_PATH[] = "..\\Bin\\Logs";
	const char LOGGER_PATH[] = "..\\Bin\\Logs\\Log.txt";

	const spdlog::level::level_enum LOG_CRITICAL = spdlog::level::critical;
	const spdlog::level::level_enum LOG_ERROR = spdlog::level::err;
	const spdlog::level::level_enum LOG_INFO = spdlog::level::info;
	const spdlog::level::level_enum LOG_DEBUG = spdlog::level::debug;

	const int LOG_LENGTH = 4096;
	const int LOG_ASYNC_MILLISECONDS = 2;

	const int MIN_PERFORMANCE_MILLISECONDS = 500;

	const WCHAR DUMP_CHECK_PATH[] = L"..\\Bin\\Dumps";
	const WCHAR DUMP_PATH[] = L"..\\Bin\\Dumps\\%d-%d-%d %d_%d_%d.dmp";
	const WCHAR DUMP_DLL_NAME[] = L"DBGHELP.DLL";
	const char DUMP_FUNCTION_NAME[] = "MiniDumpWriteDump";

	const int SPINLOCK_COUNT = 1000;

	enum E_OPERATION_TYPE : INT8
	{
		OP_NONE = 0,

		OP_SEND,
		OP_RECV,
		OP_ACCEPT,

		OP_CONNECTION,
		OP_CLOSE,
		OP_RECV_PACKET,

		OP_END
	};

	enum E_FUNCTION_RESULT : INT16
	{
		FUNCTION_RESULT_SUCCESS = 0,

		FUNCTION_RESULT_SUCCESS_POST_CLOSE_MSG = 10,

		FUNCTION_RESULT_FAIL = 100,

		FUNCTION_RESULT_FAIL_POST_CLOSE_MSG = 200,

		FUNCTION_RESULT_FAIL_MAKE_DIRECTORIES_LOG = 300,
		FUNCTION_RESULT_FAIL_MAKE_DIRECTORIES_DUMP,
		FUNCTION_RESULT_FAIL_WORKTHREAD_INFO,
		FUNCTION_RESULT_FAIL_SERVER_INFO_PORT,
		FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_RECV_OVELAPPED_BUFFER_SIZE,
		FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_SEND_OVELAPPED_BUFFER_SIZE,
		FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_RECV_CONNECTION_BUFFER_SIZE,
		FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_SEND_CONNECTION_BUFFER_SIZE,
		FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_PACKET_SIZE,
		FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_CONNECTION_COUNT,
		FUNCTION_RESULT_FAIL_SERVER_INFO_MAX_MESSAGE_POOL_COUNT,
		FUNCTION_RESULT_FAIL_SERVER_INFO_EXTRA_MESSAGE_POOL_COUNT,
		FUNCTION_RESULT_FAIL_SERVER_INFO_PERFORMANCE_PACKET_MILLISECONDS_TIME,
		FUNCTION_RESULT_FAIL_SERVER_INFO_POST_MESSAGES_THREADS_COUNT,
		FUNCTION_RESULT_FAIL_CREATE_LISTENSOCKET_STARTUP,
		FUNCTION_RESULT_FAIL_CREATE_LISTENSOCKET_SOCKET,
		FUNCTION_RESULT_FAIL_CREATE_LISTENSOCKET_BIND,
		FUNCTION_RESULT_FAIL_CREATE_LISTENSOCKET_LISTEN,
		FUNCTION_RESULT_FAIL_HANDLEIOCP_WORK,
		FUNCTION_RESULT_FAIL_HANDLEIOCP_LOGIC,
		FUNCTION_RESULT_FAIL_CREATE_MESSAGE_MANAGER,
		FUNCTION_RESULT_FAIL_LINK_IOCP,
		FUNCTION_RESULT_FAIL_CREATE_CONNECTIONS,
		FUNCTION_RESULT_FAIL_CREATE_PERFORMANCE,
		FUNCTION_RESULT_FAIL_CREATE_WORKTHREAD,

		FUNCTION_RESULT_FAIL_MESSAGE_NULL = 400,
		FUNCTION_RESULT_FAIL_PQCS,

		FUNCTION_RESULT_END
	};

	class Connection;
	struct OVERLAPPED_EX
	{
		OVERLAPPED Overlapped;
		WSABUF OverlappedExWsaBuf;

		E_OPERATION_TYPE OverlappedExOperationType;

		int	OverlappedExTotalByte;
		DWORD OverlappedExRemainByte;
		char* pOverlappedExSocketMessage;

		Connection* pOverlappedExConnection;

		OVERLAPPED_EX(Connection* pConnection)
		{
			ZeroMemory(this, sizeof(OVERLAPPED_EX));

			pOverlappedExConnection = pConnection;
		}
	};

	struct Message
	{
		E_OPERATION_TYPE OperationType = OP_NONE;
		char* pContents = nullptr;

		void Clear(void)
		{
			OperationType = OP_NONE;
			pContents = nullptr;
		}
		void SetMessage(E_OPERATION_TYPE SetOperationType, char* pSetContents)
		{
			OperationType = SetOperationType;
			pContents = pSetContents;
		}
	};

	struct CustomSpinLockCriticalSection
	{
		CRITICAL_SECTION m_CS;

		CustomSpinLockCriticalSection()
		{
			InitializeCriticalSectionAndSpinCount(&m_CS, SPINLOCK_COUNT);
		}
		~CustomSpinLockCriticalSection()
		{
			DeleteCriticalSection(&m_CS);
		}
	};

	class SpinLock
	{
	public:
		explicit SpinLock(CustomSpinLockCriticalSection* pCS)
			: m_pSpinCS(&pCS->m_CS)
		{
			EnterCriticalSection(m_pSpinCS);
		}
		~SpinLock()
		{
			LeaveCriticalSection(m_pSpinCS);
		}

	private:
		CRITICAL_SECTION* m_pSpinCS = nullptr;
	};
}