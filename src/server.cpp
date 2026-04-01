#include "server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr int kBacklog = 16;
constexpr std::size_t kBufferSize = 4096;

void printError(const std::string& action) {
    std::cerr << action << " failed: " << std::strerror(errno) << std::endl;
}

}  // namespace

Server::Server(std::uint16_t port) : port_(port), listen_fd_(-1) {}

Server::~Server() {
    closeListenSocket();
}

bool Server::start() {
    return setupListenSocket();
}

void Server::run() {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);
        const int client_fd = accept(
            listen_fd_,
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_addr_len
        );

        if (client_fd < 0) {
            printError("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN] = {0};
        const char* ip = inet_ntop(
            AF_INET,
            &client_addr.sin_addr,
            client_ip,
            sizeof(client_ip)
        );
        const std::string ip_text = ip == nullptr ? "unknown" : ip;

        std::cout << "Client connected: " << ip_text
                  << ":" << ntohs(client_addr.sin_port) << std::endl;

        handleClient(client_fd);

        if (close(client_fd) < 0) {
            printError("close client socket");
        }
    }
}

bool Server::setupListenSocket() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        printError("socket");
        return false;
    }

    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printError("setsockopt SO_REUSEADDR");
        closeListenSocket();
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_);

    if (bind(
            listen_fd_,
            reinterpret_cast<sockaddr*>(&server_addr),
            sizeof(server_addr)
        ) < 0) {
        printError("bind");
        closeListenSocket();
        return false;
    }

    if (listen(listen_fd_, kBacklog) < 0) {
        printError("listen");
        closeListenSocket();
        return false;
    }

    std::cout << "Server listening on 0.0.0.0:" << port_ << std::endl;
    return true;
}

void Server::closeListenSocket() {
    if (listen_fd_ >= 0) {
        if (close(listen_fd_) < 0) {
            printError("close listen socket");
        }
        listen_fd_ = -1;
    }
}

void Server::handleClient(int client_fd) {
    char buffer[kBufferSize];

    while (true) {
        const ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read == 0) {
            std::cout << "Client disconnected" << std::endl;
            return;
        }

        if (bytes_read < 0) {
            printError("read");
            return;
        }

        if (!writeAll(client_fd, buffer, static_cast<int>(bytes_read))) {
            return;
        }
    }
}

bool Server::writeAll(int fd, const char* data, int length) {
    int total_written = 0;
    while (total_written < length) {
        const ssize_t bytes_written = write(fd, data + total_written, length - total_written);
        if (bytes_written < 0) {
            printError("write");
            return false;
        }

        if (bytes_written == 0) {
            std::cerr << "write failed: wrote 0 bytes" << std::endl;
            return false;
        }

        total_written += static_cast<int>(bytes_written);
    }

    return true;
}
