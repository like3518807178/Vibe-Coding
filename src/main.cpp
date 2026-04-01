#include "server.h"

#include <cstdint>
#include <iostream>
#include <csignal>

int main() {
    constexpr std::uint16_t kPort = 7777;
    const char* kDbPath = "tinyim_v4.db";

    std::signal(SIGPIPE, SIG_IGN);

    Server server(kPort, kDbPath);
    if (!server.start()) {
        std::cerr << "Server startup failed" << std::endl;
        return 1;
    }

    server.run();
    return 0;
}
