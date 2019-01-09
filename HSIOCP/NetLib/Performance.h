#pragma once

#include "stdafx.h"

#include "LogManager.h"

namespace NetLib
{
	class Performance
	{
	public:
		explicit Performance(int milliseconds = 0)
		{
			if (milliseconds > MIN_PERFORMANCE_MILLISECONDS)
			{
				m_CheckPerformanceMilliseconds = milliseconds;

				SetCheckPerformance(TRUE);
				m_PerformanceThread = std::thread(&Performance::CheckPerformanceThread, this);
			}
		}
		~Performance()
		{
			if (m_IsCheckPerformance)
			{
				SetCheckPerformance(FALSE);
				m_PerformanceThread.join();
			}
		}

	public:
		void CheckPerformanceThread(void)
		{
			while (true)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(m_CheckPerformanceMilliseconds));
				if (!m_IsCheckPerformance)
				{
					return;
				}

				g_LogMgr.WriteLog(LOG_DEBUG, "Process Packet Count '{}' Per '{}' milliseconds", m_PacketProcessCount, m_CheckPerformanceMilliseconds);
				ResetPacketProcessCount();
			}
		}

		int IncrementPacketProcessCount(void) { return m_IsCheckPerformance ? InterlockedIncrement(&m_PacketProcessCount) : 0; }
		
	private:
		void SetCheckPerformance(BOOL isCheckPerformance) { InterlockedExchange(reinterpret_cast<LPLONG>(&m_IsCheckPerformance), isCheckPerformance); }
		void ResetPacketProcessCount(void) { InterlockedExchange(&m_PacketProcessCount, 0); }

	private:
		std::thread m_PerformanceThread;
		long m_PacketProcessCount = 0;
		BOOL m_IsCheckPerformance = FALSE;
		int m_CheckPerformanceMilliseconds = 0;
	};
}