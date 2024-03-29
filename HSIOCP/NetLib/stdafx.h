// stdafx.h : 자주 사용하지만 자주 변경되지는 않는
// 표준 시스템 포함 파일 또는 프로젝트 특정 포함 파일이 들어 있는
// 포함 파일입니다.
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

// 여기서 프로그램에 필요한 추가 헤더를 참조합니다.

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock.lib")  

#include <iostream>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <mswsock.h>
#include <process.h>
#include <time.h>
#include <memory>

#include <vector>
#include <list>
#include <map>
#include <unordered_set>
#include <concurrent_queue.h>
#include <mutex>
#include <thread>
#include <string>
#include <chrono>

#include <DbgHelp.h>
typedef BOOL(WINAPI *MINIDUMPWRITEDUMP)(
	HANDLE hProcess,
	DWORD dwPid,
	HANDLE hFile,
	MINIDUMP_TYPE DumpType,
	CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

#include "spdlog/spdlog.h"

#include "CommonDefine.h"