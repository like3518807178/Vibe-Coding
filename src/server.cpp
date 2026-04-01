#include "server.h"

#include "../protocol/FramingCodec.h"
#include "../protocol/JsonCodec.h"
#include "../service/AuthService.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr int kBacklog = 16;
constexpr std::size_t kBufferSize = 4096;

void printError(const std::string& action) {
    std::cerr << action << " failed: " << std::strerror(errno) << std::endl;
}

}  // namespace

Server::Server(std::uint16_t port, std::string db_path)
    : port_(port),
      db_path_(std::move(db_path)),
      listen_fd_(-1),
      auth_service_(std::make_unique<AuthService>(db_path_)) {}

Server::~Server() {
    for (const auto& entry : clients_) {
        if (close(entry.first) < 0) {
            printError("close client socket");
        }
    }

    closeListenSocket();
}

bool Server::start() {
    std::string db_error;
    if (!auth_service_->init(db_error)) {
        std::cerr << "Auth service init failed: " << db_error << std::endl;
        return false;
    }

    return setupListenSocket();
}

void Server::run() {
    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_fd_, &read_fds);

        for (const auto& entry : clients_) {
            FD_SET(entry.first, &read_fds);
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

        std::vector<int> ready_clients;
        for (const auto& entry : clients_) {
            if (FD_ISSET(entry.first, &read_fds)) {
                ready_clients.push_back(entry.first);
            }
        }

        for (int client_fd : ready_clients) {
            if (clients_.find(client_fd) != clients_.end()) {
                handleClientReadable(client_fd);
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

    std::cout << "Framed chat server listening on 0.0.0.0:" << port_ << std::endl;
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

    clients_.emplace(client_fd, ClientConnection{client_fd, ""});
    std::cout << "Client connected: " << ip_text
              << ":" << ntohs(client_addr.sin_port)
              << ", fd=" << client_fd << std::endl;
    return true;
}

void Server::handleClientReadable(int client_fd) {
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

    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return;
    }

    it->second.inbuf.append(buffer, static_cast<std::size_t>(bytes_read));
    if (it->second.inbuf.size() > protocol::FramingCodec::kMaxFrameSize + sizeof(std::uint32_t)) {
        std::cerr << "Client sent oversized buffered data: fd=" << client_fd << std::endl;
        removeClient(client_fd);
        return;
    }

    if (!processFrames(client_fd)) {
        removeClient(client_fd);
    }
}

bool Server::processFrames(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return false;
    }

    while (true) {
        std::string frame;
        std::string frame_error;
        const bool got_frame = protocol::FramingCodec::tryPopFrame(it->second.inbuf, frame, frame_error);
        if (!frame_error.empty()) {
            std::cerr << "Invalid frame from fd=" << client_fd
                      << ": " << frame_error << std::endl;
            return false;
        }

        if (!got_frame) {
            return true;
        }

        std::map<std::string, std::string> fields;
        std::string json_error;
        if (!protocol::JsonCodec::parseObject(frame, fields, json_error)) {
            std::cerr << "JSON body rejected by V3 protocol from fd=" << client_fd
                      << ": " << json_error << std::endl;
            return false;
        }

        std::cout << "Parsed JSON frame from fd=" << client_fd << std::endl;
        if (!handleApplicationMessage(client_fd, frame)) {
            return false;
        }
    }
}

bool Server::handleApplicationMessage(int client_fd, const std::string& json_text) {
    std::map<std::string, std::string> fields;
    std::string parse_error;
    if (!protocol::JsonCodec::parseObject(json_text, fields, parse_error)) {
        std::cerr << "Failed to parse application JSON from fd=" << client_fd
                  << ": " << parse_error << std::endl;
        return false;
    }

    bool handled = false;
    const std::map<std::string, std::string> response = auth_service_->handleMessage(fields, handled);
    if (handled) {
        return sendJsonMessage(client_fd, protocol::JsonCodec::encodeObject(response));
    }

    broadcastJsonMessage(client_fd, json_text);
    return true;
}

bool Server::sendJsonMessage(int client_fd, const std::string& json_text) {
    const std::string packet = protocol::FramingCodec::encode(json_text);
    if (!writeAll(client_fd, packet.data(), packet.size())) {
        std::cerr << "Failed to send response to fd=" << client_fd << std::endl;
        return false;
    }

    return true;
}

void Server::removeClient(int client_fd) {
    if (clients_.erase(client_fd) == 0) {
        return;
    }

    if (close(client_fd) < 0) {
        printError("close client socket");
    }
}

void Server::broadcastJsonMessage(int sender_fd, const std::string& json_text) {
    const std::string packet = protocol::FramingCodec::encode(json_text);

    std::vector<int> failed_clients;
    for (const auto& entry : clients_) {
        const int client_fd = entry.first;
        if (client_fd == sender_fd) {
            continue;
        }

        if (!writeAll(client_fd, packet.data(), packet.size())) {
            failed_clients.push_back(client_fd);
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
    for (const auto& entry : clients_) {
        max_fd = std::max(max_fd, entry.first);
    }

    return max_fd;
}
