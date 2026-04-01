#include "server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <string>
#include <sys/select.h>
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
    for (int client_fd : client_fds_) {
        if (close(client_fd) < 0) {
            printError("close client socket");
        }
    }

    closeListenSocket();
}

bool Server::start() {
    return setupListenSocket();
}

void Server::run() {
    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_fd_, &read_fds);

        for (int client_fd : client_fds_) {
            FD_SET(client_fd, &read_fds);
        }

        const int ready_count = select(getMaxFd() + 1, &read_fds, nullptr, nullptr, nullptr);
        if (ready_count < 0) {
            printError("select");
            continue;
        }

        if (FD_ISSET(listen_fd_, &read_fds)) {
            if (!handleNewConnection()) {
                continue;
            }
        }

        std::set<int> ready_clients;
        for (int client_fd : client_fds_) {
            if (FD_ISSET(client_fd, &read_fds)) {
                ready_clients.insert(client_fd);
            }
        }

        for (int client_fd : ready_clients) {
            if (client_fds_.find(client_fd) != client_fds_.end()) {
                handleClientMessage(client_fd);
            }
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

    std::cout << "Chat server listening on 0.0.0.0:" << port_ << std::endl;
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

bool Server::handleNewConnection() {
    sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);
    const int client_fd = accept(
        listen_fd_,
        reinterpret_cast<sockaddr*>(&client_addr),
        &client_addr_len
    );

    if (client_fd < 0) {
        printError("accept");
        return false;
    }

    char client_ip[INET_ADDRSTRLEN] = {0};
    const char* ip = inet_ntop(
        AF_INET,
        &client_addr.sin_addr,
        client_ip,
        sizeof(client_ip)
    );
    const std::string ip_text = ip == nullptr ? "unknown" : ip;

    client_fds_.insert(client_fd);
    std::cout << "Client connected: " << ip_text
              << ":" << ntohs(client_addr.sin_port)
              << ", fd=" << client_fd << std::endl;
    return true;
}

void Server::handleClientMessage(int client_fd) {
    char buffer[kBufferSize];
    const ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
    if (bytes_read == 0) {
        std::cout << "Client disconnected: fd=" << client_fd << std::endl;
        removeClient(client_fd);
        return;
    }

    if (bytes_read < 0) {
        printError("read");
        removeClient(client_fd);
        return;
    }

    broadcastMessage(client_fd, buffer, static_cast<std::size_t>(bytes_read));
}

void Server::removeClient(int client_fd) {
    if (client_fds_.erase(client_fd) == 0) {
        return;
    }

    if (close(client_fd) < 0) {
        printError("close client socket");
    }
}

void Server::broadcastMessage(int sender_fd, const char* data, int length) {
    std::set<int> failed_clients;
    for (int client_fd : client_fds_) {
        if (client_fd == sender_fd) {
            continue;
        }

        if (!writeAll(client_fd, data, static_cast<std::size_t>(length))) {
            failed_clients.insert(client_fd);
        }
    }

    for (int client_fd : failed_clients) {
        std::cerr << "Removing client after write failure: fd=" << client_fd << std::endl;
        removeClient(client_fd);
    }
}

bool Server::writeAll(int fd, const char* data, std::size_t length) {
    std::size_t total_written = 0;
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

        total_written += static_cast<std::size_t>(bytes_written);
    }

    return true;
}

int Server::getMaxFd() const {
    int max_fd = listen_fd_;
    for (int client_fd : client_fds_) {
        max_fd = std::max(max_fd, client_fd);
    }

    return max_fd;
}
