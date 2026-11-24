#include "Server.h"
#include <cstdlib>
#include <ctime>
#include <iostream>

// 사용법: CrazyArcadeServer.exe [포트] [바인드 IP]
//   인자 없이 실행하면 0.0.0.0:9000 (LAN 테스트용)
//   한 PC 테스트는 "CrazyArcadeServer.exe 9000 127.0.0.1"로 실행하면 방화벽 창이 뜨지 않는다
int main(int argc, char* argv[])
{
    SetConsoleOutputCP(CP_UTF8);   // 닉네임(UTF-8)이 콘솔에 제대로 보이도록
    std::srand(static_cast<unsigned>(std::time(nullptr)));   // GameWorld의 블록·아이템 배치가 판마다 달라지도록
    std::cout << "=== Crazy Arcade Online Server ===" << std::endl;

    unsigned short port = static_cast<unsigned short>(SERVER_PORT);
    if (argc >= 2) {
        int p = std::atoi(argv[1]);
        if (p <= 0 || p > 65535) {
            std::cout << "[error] invalid port: " << argv[1] << std::endl;
            return 1;
        }
        port = static_cast<unsigned short>(p);
    }
    const char* bindIp = (argc >= 3) ? argv[2] : nullptr;

    Server server;
    if (!server.Init(port, bindIp)) {
        server.Shutdown();
        return 1;
    }

    server.Run();
    server.Shutdown();
    return 0;
}
