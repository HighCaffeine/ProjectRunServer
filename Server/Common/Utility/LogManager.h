#pragma once

#define SPDLOG_HEADER_ONLY
#include "spdlog/spdlog.h"
#include "spdlog/sinks/daily_file_sink.h"		// 날짜별 출력용
#include "spdlog/sinks/stdout_color_sinks.h"	// 콘솔 출력용
#include "spdlog/sinks/msvc_sink.h"				// vs 출력용

class LogManager
{
private:

private:

public:
	static void Init()
	{
		try
		{
			//콘솔 출력 설정
			auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			consoleSink->set_level(spdlog::level::debug);

			//날짜 파일 저장
			auto fileSink = std::make_shared<spdlog::sinks::daily_file_sink_mt>("logs/server.txt", 0, 0);
			fileSink->set_level(spdlog::level::info);

			//vs 
			auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
			msvcSink->set_level(spdlog::level::debug);

			//로거 등록
			spdlog::sinks_init_list sinkList = { consoleSink, fileSink, msvcSink };
			auto logger = std::make_shared<spdlog::logger>("Server", sinkList.begin(), sinkList.end());

			//[2026-02-16 12:34:56] [info] 로그 내용
			logger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
			spdlog::set_default_logger(logger);
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			printf("[spdlog] log init failed : %s\n", ex.what());
		}
	}
};