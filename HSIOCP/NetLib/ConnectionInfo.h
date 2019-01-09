#pragma once

#include "stdafx.h"

#include "RingBuffer.h"

namespace NetLib
{
	struct ConnectionInfo
	{
		OVERLAPPED_EX* pRecvOverlappedEx = nullptr;
		OVERLAPPED_EX* pSendOverlappedEx = nullptr;

		RingBuffer RingRecvBuffer;
		RingBuffer RingSendBuffer;

		char AddrBuf[MAX_ADDR_LENGTH] = { 0, };

		BOOL IsClosed = FALSE;
		BOOL IsConnect = FALSE;
		BOOL IsSendable = TRUE;

		void Init(void)
		{
			RingRecvBuffer.Init();
			RingSendBuffer.Init();

			IsConnect = FALSE;
			IsClosed = FALSE;
			IsSendable = TRUE;
		}
	};
}