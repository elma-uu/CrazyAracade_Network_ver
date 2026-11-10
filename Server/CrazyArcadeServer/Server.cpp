#include "Server.h"
#include <iostream>

bool Server::Init(unsigned short port)
{
    // 11/1 구현 예정: 논블로킹 listen
    std::cout << "[server] init (port " << port << ")" << std::endl;
    return true;
}

void Server::Run()
{
    // 11/1 구현 예정: 논블로킹 메인 루프
}

void Server::Shutdown()
{
    std::cout << "[server] shutdown" << std::endl;
}
