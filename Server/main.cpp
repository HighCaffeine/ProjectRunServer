#include "GameServer.h"
#include "sentry.h"

#include <string>
#include <iostream>

const UINT16 SERVER_PORT = 11021;
const UINT16 MAX_CLIENT = 3;		//총 접속할수 있는 클라이언트 수
const UINT32 MAX_IO_WORKER_THREAD = 4;  //쓰레드 풀에 넣을 쓰레드 수

int main()
{
	GameServer server;

	sentry_options_t* options = sentry_options_new();
	sentry_options_set_dsn(options, "https://79824d5b1c51a97749e88cf8667b0b7c@o4510992232873984.ingest.us.sentry.io/4510992622026752");
	// This is also the default-path. For further information and recommendations:
	// https://docs.sentry.io/platforms/native/configuration/options/#database_path
	sentry_options_set_database_path(options, ".sentry-native");
	sentry_options_set_release(options, "1.0.0");
	sentry_options_set_debug(options, 1);
	sentry_init(options);

	sentry_capture_event(sentry_value_new_message_event(
		/*   level */ SENTRY_LEVEL_INFO,
		/*  logger */ "custom",
		/* message */ "It works2!"
	));
	SetConsoleOutputCP(CP_UTF8);
	//SetConsoleOutputCP(65001);

	//소켓을 초기화
	server.Init(MAX_IO_WORKER_THREAD);

	//소켓과 서버 주소를 연결하고 등록 시킨다.
	server.BindandListen(SERVER_PORT);

	server.Run(MAX_CLIENT);

	printf("아무 키나 누를 때까지 대기합니다\n");
	while (true)
	{
		std::string inputCmd;
		std::getline(std::cin, inputCmd);
		if (std::cin.eof() || std::cin.fail())
		{
			std::cin.clear();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		if (inputCmd == "quit")
		{
			break;
		}
	}

	server.End();
	sentry_close();
	return 0;
}

