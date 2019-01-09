#pragma once

#include "stdafx.h"

namespace NetLib
{
	class LogManager
	{
	public:
		LogManager()
		{
			spdlog::set_async_mode(LOG_LENGTH,
				spdlog::async_overflow_policy::block_retry,
				nullptr,
				std::chrono::seconds(LOG_ASYNC_MILLISECONDS));

			m_Logger = spdlog::daily_logger_mt(LOGGER_NAME, LOGGER_PATH, 0, 0);
			m_Logger->set_level(LOG_DEBUG);

			m_ConsoleLogger = spdlog::get("console");
			if (!m_ConsoleLogger)
			{
				m_ConsoleLogger = spdlog::stdout_color_mt("console");
			}
			m_ConsoleLogger->set_level(LOG_INFO);
		}

	public:
		static LogManager& GetInstance(void)
		{
			std::call_once(m_OnceFlag, [] {m_pInstance.reset(new LogManager); });

			return *m_pInstance.get();
		}

		template<typename... Args>
		void WriteLog(spdlog::level::level_enum level, const char* pLog, const Args &... args)
		{
			m_ConsoleLogger->log(level, pLog, args...);
			m_Logger->log(level, pLog, args...);
		}

	private:
		static std::unique_ptr<LogManager> m_pInstance;
		static std::once_flag m_OnceFlag;

		std::shared_ptr<spdlog::logger> m_Logger;
		std::shared_ptr<spdlog::logger> m_ConsoleLogger;
	};
}

#define g_LogMgr NetLib::LogManager::GetInstance()