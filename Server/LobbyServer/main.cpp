#include "LobbyServer.h"
#include "sentry.h"

#include <string>
#include <iostream>
#include <spdlog/details/os-inl.h>

class LobbyServer;

const UINT16 SERVER_PORT = 11020;
const UINT16 MAX_CLIENT = 100;    // 로비는 게임서버보다 접속 인원을 넉넉하게 잡음
const UINT32 MAX_IO_WORKER_THREAD = 4;

int main()
{
	LobbyServer server;

	sentry_options_t* options = sentry_options_new();
	sentry_options_set_dsn(options, "https://79824d5b1c51a97749e88cf8667b0b7c@o4510992232873984.ingest.us.sentry.io/4510992622026752");
	// This is also the default-path. For further information and recommendations:
	// https://docs.sentry.io/platforms/native/configuration/options/#database_path
	sentry_options_set_database_path(options, ".sentry-native-lobby");
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

	server.Init(MAX_IO_WORKER_THREAD);
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

