#include "server.h"

#include <cstdint>
#include <iostream>
#include <csignal>

int main() {
    constexpr std::uint16_t kPort = 7777;

    std::signal(SIGPIPE, SIG_IGN);

    Server server(kPort);
    if (!server.start()) {
        std::cerr << "Server startup failed" << std::endl;
        return 1;
    }

    server.run();
    return 0;
}
