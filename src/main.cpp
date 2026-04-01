#include "server.h"

#include "../common/Config.h"
#include "../common/Logger.h"
#include "../protocol/FramingCodec.h"

#include <cstdint>
#include <csignal>
#include <iostream>

int main() {
    const std::string config_path = "config/server.conf";
    AppConfig config;
    std::string config_error;
    if (!Config::loadFromFile(config_path, config, config_error)) {
        std::cerr << "Config load failed: " << config_error << std::endl;
        return 1;
    }

    Logger::setLevel(config.log_level);
    Logger::info(Config::toDisplayString(config));
    protocol::FramingCodec::setMaxFrameSize(config.max_packet_size);

    std::signal(SIGPIPE, SIG_IGN);

    Server server(config);
    if (!server.start()) {
        Logger::error("Server startup failed");
        return 1;
    }

    server.run();
    return 0;
}
