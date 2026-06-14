#include "GameServer.h"
#include "sentry.h"

#include <string>
#include <iostream>
#include <conio.h>
#include <thread>

const UINT32 MAX_IO_WORKER_THREAD = 4;
UINT16 g_ServerPort = 11021;
bool g_IsServerRunning = true;

int main(int argc, char* argv[])
{
    UINT16 serverPort = 11021;
    INT32 roomNumber = 0;

    if (argc >= 3)
    {
        serverPort = (UINT16)std::stoi(argv[1]);
        roomNumber = std::stoi(argv[2]);
        g_ServerPort = serverPort;
        printf("[System] 로비로부터 전달받은 정보 -> 포트: %d, 방번호: %d\n", serverPort, roomNumber);
    }
    else
    {
        printf("[System] 인자가 없어 기본 포트(%d)로 구동.\n", serverPort);
    }

    GameServer server;

    // Sentry 초기화는 그대로 유지
    sentry_options_t* options = sentry_options_new();
    sentry_options_set_dsn(options, "https://79824d5b1c51a97749e88cf8667b0b7c@o4510992232873984.ingest.us.sentry.io/4510992622026752");
    sentry_options_set_database_path(options, ".sentry-native-game");
    sentry_options_set_release(options, "1.0.0");
    sentry_options_set_debug(options, 1);
    sentry_init(options);

    SetConsoleOutputCP(CP_UTF8);

    server.Init(MAX_IO_WORKER_THREAD);
    server.BindandListen(serverPort);

    // 예: server.SetRoomNumber(roomNumber);

    server.Run(10, g_ServerPort);

    printf("아무 키나 누를 때까지 대기합니다\n");

    while (g_IsServerRunning)
    {
        if (_kbhit())
        {
            int ch = _getch();
            if (ch == 'q' || ch == 'Q')
            {
                printf("[System] 수동 종료 요청 확인.\n");
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.End();
    sentry_close();
    return 0;
}