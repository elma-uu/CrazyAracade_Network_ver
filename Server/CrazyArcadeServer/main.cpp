#include "Server.h"
#include <iostream>

int main()
{
    std::cout << "=== Crazy Arcade Online Server ===" << std::endl;

    Server server;
    if (!server.Init(SERVER_PORT))
        return 1;

    server.Run();
    server.Shutdown();
    return 0;
}
