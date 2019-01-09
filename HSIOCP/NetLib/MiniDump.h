#pragma once

#include "stdafx.h"

namespace NetLib
{
	extern LPTOP_LEVEL_EXCEPTION_FILTER g_previousExceptionFilter;
	LPTOP_LEVEL_EXCEPTION_FILTER g_previousExceptionFilter = nullptr;

	DWORD WINAPI WriteDump(LPVOID pParam)
	{
		auto pExceptionInfo = reinterpret_cast<PEXCEPTION_POINTERS>(pParam);

		auto dllHandle = LoadLibrary(DUMP_DLL_NAME);
		if (dllHandle != nullptr)
		{
			MINIDUMPWRITEDUMP dump = (MINIDUMPWRITEDUMP)GetProcAddress(dllHandle, DUMP_FUNCTION_NAME);
			if (dump != nullptr)
			{
				WCHAR szDumpPath[MAX_PATH] = { 0, };
				SYSTEMTIME systemTime;
				GetLocalTime(&systemTime);

				swprintf_s(szDumpPath,
					MAX_PATH,
					DUMP_PATH,
					systemTime.wYear,
					systemTime.wMonth,
					systemTime.wDay,
					systemTime.wHour,
					systemTime.wMinute,
					systemTime.wSecond);

				auto hFileHandle = CreateFile(
					szDumpPath,
					GENERIC_WRITE,
					FILE_SHARE_WRITE,
					NULL,
					CREATE_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL);

				if (hFileHandle != INVALID_HANDLE_VALUE)
				{
					_MINIDUMP_EXCEPTION_INFORMATION miniDumpExceptionInfo;

					miniDumpExceptionInfo.ThreadId = GetCurrentThreadId();
					miniDumpExceptionInfo.ExceptionPointers = pExceptionInfo;
					miniDumpExceptionInfo.ClientPointers = NULL;

					auto isSuccess = dump(
						GetCurrentProcess(),
						GetCurrentProcessId(),
						hFileHandle,
						MiniDumpNormal,
						&miniDumpExceptionInfo,
						NULL,
						NULL);

					if (isSuccess == TRUE)
					{
						CloseHandle(hFileHandle);

						return EXCEPTION_EXECUTE_HANDLER;
					}
				}
				CloseHandle(hFileHandle);
			}
		}
		return EXCEPTION_CONTINUE_SEARCH;
	}

	LONG WINAPI UnHandledExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo)
	{
		if (pExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
		{
			std::thread overflowThread = std::thread(WriteDump, pExceptionInfo);
			overflowThread.join();
		}
		else
		{
			return WriteDump(pExceptionInfo);
		}
		return EXCEPTION_EXECUTE_HANDLER;
	}

	class MiniDump
	{
	public:
		static void Begin(void)
		{
			SetErrorMode(SEM_FAILCRITICALERRORS);
			g_previousExceptionFilter = SetUnhandledExceptionFilter(UnHandledExceptionFilter);
		}
		static void End(void)
		{
			SetUnhandledExceptionFilter(g_previousExceptionFilter);
		}
	};
}