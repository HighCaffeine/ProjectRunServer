#pragma once

#define SPDLOG_HEADER_ONLY
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/msvc_sink.h"

#include <iomanip>
#include <ctime>
#include <sstream>
#include <filesystem> // 폴더 생성을 위해 추가

class LogManager
{
public:
	// serverPrefix: Lobby / Game
	// identifier: 게임 서버일 경우 방 번호나 포트 번호 (기본값 -1)

	static void Init(const std::string& serverPrefix = "Lobby", int identifier = -1)
	{
		try
		{
			//  로그 폴더 생성 (Lobby, Game 분리)
			std::string folderPath = "logs/" + serverPrefix;
			std::filesystem::create_directories(folderPath); // 폴더가 없으면 자동 생성

			// 현재 시간 가져오기
			auto t = std::time(nullptr);
			auto tm = *std::localtime(&t);

			// 파일명 포맷 (logs/Game/Game_Port7777_20260614_153000.log)
			std::ostringstream oss;
			oss << folderPath << "/" << serverPrefix;

			if (identifier != -1)
			{
				oss << "_Port" << identifier; // 포트나 방 번호 추가
			}

			//최종 파일명 세팅
			oss << "_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".log";

			//콘솔 출력 설정 
			auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();	//멀티쓰레드 환경
			consoleSink->set_level(spdlog::level::debug);	//debug 이상 설정

			//텍스트 파일 설정 
			//위에서 설정한 파일명, true -> 기존 파일이 있다면 이어쓰기
			auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(oss.str(), true);	
			fileSink->set_level(spdlog::level::info);	// info 이상만 설정 (debug까지 찍으면 너무 많음)

			//vs 출력 설정
			auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
			msvcSink->set_level(spdlog::level::debug);

			//통합 로거 싱크 세팅
			spdlog::sinks_init_list sinkList = { consoleSink, fileSink, msvcSink };
			auto logger = std::make_shared<spdlog::logger>("Server", sinkList.begin(), sinkList.end());

			logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");	//로깅 포맷
			spdlog::set_default_logger(logger);	//기본 로거 세팅

			spdlog::set_level(spdlog::level::debug);	//최소 레벨 설정
			spdlog::flush_on(spdlog::level::err);	//오류 발생해도 강제 저장 설정
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			printf("[spdlog] log init failed : %s\n", ex.what());
		}
	}

	static void LogBandwidth(UINT32 totalRecvBytes, UINT32 totalSendBytes, UINT64 grandTotalRecv, UINT64 grandTotalSend)
	{
		double recvKBps = totalRecvBytes / 1024.0;
		double sendKBps = totalSendBytes / 1024.0;
		double totalRecvMB = grandTotalRecv / (1024.0 * 1024.0);
		double totalSendMB = grandTotalSend / (1024.0 * 1024.0);

		if (totalRecvBytes > 0 || totalSendBytes > 0)
		{
			spdlog::info("[Bandwidth] In: {:.2f} KB/s | Out: {:.2f} KB/s  ||  Total - In: {:.2f} MB | Out: {:.2f} MB",
				recvKBps, sendKBps, totalRecvMB, totalSendMB);
		}
	}

	static void LogFinalSummary(UINT64 grandTotalRecv, UINT64 grandTotalSend, int peakCCU = 0)
	{
		double finalRecvMB = grandTotalRecv / (1024.0 * 1024.0);
		double finalSendMB = grandTotalSend / (1024.0 * 1024.0);

		spdlog::info("==================================================");
		spdlog::info("[Server Final Summary]");
		spdlog::info("- Total Data Received : {:.2f} MB", finalRecvMB);
		spdlog::info("- Total Data Sent     : {:.2f} MB", finalSendMB);
		spdlog::info("- Peak Concurrent Users : {} users", peakCCU);
		spdlog::info("==================================================");

		spdlog::shutdown();
	}
};