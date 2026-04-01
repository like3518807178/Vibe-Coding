#include "server.h"

#include "../net/EpollReactor.h"
#include "../protocol/FramingCodec.h"
#include "../protocol/JsonCodec.h"
#include "../service/AuthService.h"
#include "../service/OfflineService.h"
#include "../service/SessionManager.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr int kBacklog = 16;
constexpr std::size_t kBufferSize = 4096;
constexpr int kMaxEvents = 64;

void printError(const std::string& action) {
    std::cerr << action << " failed: " << std::strerror(errno) << std::endl;
}

bool isWouldBlockError() {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

}  // namespace

Server::Server(std::uint16_t port, std::string db_path)
    : port_(port),
      db_path_(std::move(db_path)),
      listen_fd_(-1),
      reactor_(std::make_unique<EpollReactor>()),
      auth_service_(std::make_unique<AuthService>(db_path_)),
      session_manager_(std::make_unique<SessionManager>()),
      offline_service_(std::make_unique<OfflineService>(db_path_, *session_manager_)) {}

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

    if (!offline_service_->init(db_error)) {
        std::cerr << "Offline service init failed: " << db_error << std::endl;
        return false;
    }

    if (!reactor_->create()) {
        printError("epoll_create1");
        return false;
    }

    return setupListenSocket();
}

void Server::run() {
    std::vector<epoll_event> events(kMaxEvents);

    while (true) {
        const int ready_count = reactor_->wait(events, -1);
        if (ready_count < 0) {
            if (errno == EINTR) {
                continue;
            }

            printError("epoll_wait");
            continue;
        }

        for (int i = 0; i < ready_count; ++i) {
            const int fd = events[i].data.fd;
            const std::uint32_t event_mask = events[i].events;

            if (fd == listen_fd_) {
                if (!handleAccept()) {
                    continue;
                }
                continue;
            }

            if (clients_.find(fd) == clients_.end()) {
                continue;
            }

            if ((event_mask & (EPOLLERR | EPOLLHUP)) != 0U) {
                removeClient(fd);
                continue;
            }

            if ((event_mask & EPOLLIN) != 0U && clients_.find(fd) != clients_.end()) {
                handleClientReadable(fd);
            }

            if ((event_mask & EPOLLOUT) != 0U && clients_.find(fd) != clients_.end()) {
                if (!flushClientWrites(fd)) {
                    removeClient(fd);
                }
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

    if (!setNonBlocking(listen_fd_)) {
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

    if (!reactor_->add(listen_fd_, EPOLLIN)) {
        printError("epoll_ctl add listen fd");
        closeListenSocket();
        return false;
    }

    std::cout << "Epoll chat server listening on 0.0.0.0:" << port_ << std::endl;
    return true;
}

bool Server::setNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        printError("fcntl F_GETFL");
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        printError("fcntl F_SETFL O_NONBLOCK");
        return false;
    }

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

bool Server::updateClientEvents(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return false;
    }

    std::uint32_t events = EPOLLIN;
    if (!it->second.outbuf.empty()) {
        events |= EPOLLOUT;
    }

    if (!reactor_->modify(client_fd, events)) {
        printError("epoll_ctl mod client fd");
        return false;
    }

    return true;
}

bool Server::handleAccept() {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);
        const int client_fd = accept(
            listen_fd_,
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_addr_len
        );

        if (client_fd < 0) {
            if (isWouldBlockError()) {
                return true;
            }

            printError("accept");
            return false;
        }

        if (!setNonBlocking(client_fd)) {
            close(client_fd);
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

        clients_.emplace(client_fd, ClientConnection{client_fd, "", "", ConnectionState::Connected});
        if (!reactor_->add(client_fd, EPOLLIN)) {
            printError("epoll_ctl add client fd");
            clients_.erase(client_fd);
            close(client_fd);
            continue;
        }

        std::cout << "Client connected: " << ip_text
                  << ":" << ntohs(client_addr.sin_port)
                  << ", fd=" << client_fd << std::endl;
    }
}

void Server::handleClientReadable(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return;
    }

    char buffer[kBufferSize];
    while (true) {
        const ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read == 0) {
            std::cout << "Client disconnected: fd=" << client_fd << std::endl;
            removeClient(client_fd);
            return;
        }

        if (bytes_read < 0) {
            if (isWouldBlockError()) {
                return;
            }

            printError("read");
            removeClient(client_fd);
            return;
        }

        it = clients_.find(client_fd);
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
            return;
        }
    }
}

bool Server::flushClientWrites(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return false;
    }

    while (!it->second.outbuf.empty()) {
        const ssize_t bytes_written = write(
            client_fd,
            it->second.outbuf.data(),
            it->second.outbuf.size()
        );

        if (bytes_written < 0) {
            if (isWouldBlockError()) {
                return true;
            }

            printError("write");
            return false;
        }

        if (bytes_written == 0) {
            std::cerr << "write failed: wrote 0 bytes" << std::endl;
            return false;
        }

        it->second.outbuf.erase(0, static_cast<std::size_t>(bytes_written));
    }

    return updateClientEvents(client_fd);
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
            std::cerr << "JSON body rejected by V6 protocol from fd=" << client_fd
                      << ": " << json_error << std::endl;
            return false;
        }

        std::cout << "Parsed JSON frame from fd=" << client_fd << std::endl;
        if (!handleApplicationMessage(client_fd, frame)) {
            return false;
        }

        it = clients_.find(client_fd);
        if (it == clients_.end()) {
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

    const AuthResult auth_result = auth_service_->handleMessage(fields);
    if (auth_result.handled) {
        std::map<std::string, std::string> response = {
            {"message", auth_result.message},
            {"ok", auth_result.success ? "true" : "false"},
            {"type", auth_result.type}
        };

        if (auth_result.type == "login_resp" && auth_result.success) {
            if (session_manager_->isUserOnline(auth_result.username)) {
                response["ok"] = "false";
                response["message"] = "用户已在线，当前策略为拒绝新登录";
            } else if (!session_manager_->bindUser(auth_result.username, client_fd)) {
                response["ok"] = "false";
                response["message"] = "绑定在线态失败";
            } else {
                auto it = clients_.find(client_fd);
                if (it != clients_.end()) {
                    it->second.state = ConnectionState::Authed;
                }
            }
        }

        if (!sendJsonObject(client_fd, response)) {
            return false;
        }

        if (auth_result.type == "login_resp" && response["ok"] == "true") {
            return deliverOfflineMessages(client_fd, auth_result.username);
        }

        return true;
    }

    if (!session_manager_->isAuthed(client_fd) || getConnectionState(client_fd) != ConnectionState::Authed) {
        return sendJsonObject(
            client_fd,
            {
                {"message", "未登录用户不能发送业务消息"},
                {"ok", "false"},
                {"type", "error_resp"}
            }
        );
    }

    auto type_it = fields.find("type");
    if (type_it == fields.end()) {
        return sendJsonObject(
            client_fd,
            {
                {"message", "缺少 type"},
                {"ok", "false"},
                {"type", "error_resp"}
            }
        );
    }

    if (type_it->second != "send") {
        return sendJsonObject(
            client_fd,
            {
                {"message", "未知业务消息类型"},
                {"ok", "false"},
                {"type", "error_resp"}
            }
        );
    }

    const auto to_it = fields.find("to");
    const auto text_it = fields.find("text");
    if (to_it == fields.end() || text_it == fields.end()) {
        return sendJsonObject(
            client_fd,
            {
                {"message", "缺少 to 或 text"},
                {"ok", "false"},
                {"type", "send_resp"}
            }
        );
    }

    const std::string from_user = session_manager_->getUserByFd(client_fd);
    SendDispatchResult dispatch{};
    std::string offline_error;
    if (!offline_service_->handleSend(from_user, to_it->second, text_it->second, dispatch, offline_error)) {
        return sendJsonObject(
            client_fd,
            {
                {"message", offline_error.empty() ? "发送失败" : offline_error},
                {"ok", "false"},
                {"type", "send_resp"}
            }
        );
    }

    if (dispatch.deliver_online) {
        if (!sendJsonObject(dispatch.target_fd, dispatch.deliver_payload)) {
            return false;
        }
    }

    return sendJsonObject(
        client_fd,
        {
            {"message", dispatch.message},
            {"ok", dispatch.success ? "true" : "false"},
            {"type", "send_resp"}
        }
    );
}

bool Server::sendJsonMessage(int client_fd, const std::string& json_text) {
    const std::string packet = protocol::FramingCodec::encode(json_text);
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return false;
    }

    if (it->second.outbuf.empty()) {
        const ssize_t bytes_written = write(client_fd, packet.data(), packet.size());
        if (bytes_written < 0) {
            if (!isWouldBlockError()) {
                printError("write");
                return false;
            }

            it->second.outbuf = packet;
            return updateClientEvents(client_fd);
        }

        if (bytes_written == 0) {
            std::cerr << "write failed: wrote 0 bytes" << std::endl;
            return false;
        }

        if (static_cast<std::size_t>(bytes_written) == packet.size()) {
            return true;
        }

        it->second.outbuf.append(packet.substr(static_cast<std::size_t>(bytes_written)));
        return updateClientEvents(client_fd);
    }

    it->second.outbuf.append(packet);
    return updateClientEvents(client_fd);
}

bool Server::sendJsonObject(int client_fd, const std::map<std::string, std::string>& fields) {
    return sendJsonMessage(client_fd, protocol::JsonCodec::encodeObject(fields));
}

bool Server::deliverOfflineMessages(int client_fd, const std::string& username) {
    std::vector<OfflineMessageRecord> records;
    std::string error;
    if (!offline_service_->listUndelivered(username, records, error)) {
        std::cerr << "Failed to list offline messages for " << username
                  << ": " << error << std::endl;
        return false;
    }

    for (const auto& record : records) {
        if (!sendJsonObject(
                client_fd,
                {
                    {"from", record.from_user},
                    {"msg_id", std::to_string(record.msg_id)},
                    {"text", record.body},
                    {"to", record.to_user},
                    {"ts", record.ts},
                    {"type", "offline_msg"}
                }
            )) {
            return false;
        }

        if (!offline_service_->markDelivered(record.msg_id, error)) {
            std::cerr << "Failed to mark offline message delivered, msg_id="
                      << record.msg_id << ": " << error << std::endl;
            return false;
        }
    }

    return true;
}

ConnectionState Server::getConnectionState(int client_fd) const {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return ConnectionState::Closed;
    }

    return it->second.state;
}

void Server::removeClient(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return;
    }

    it->second.state = ConnectionState::Closed;
    session_manager_->unbindFd(client_fd);
    reactor_->remove(client_fd);
    clients_.erase(it);

    if (close(client_fd) < 0) {
        printError("close client socket");
    }
}

bool Server::writeAll(int fd, const char* data, std::size_t length) {
    std::size_t total_written = 0;
    while (total_written < length) {
        const ssize_t bytes_written = write(fd, data + total_written, length - total_written);
        if (bytes_written < 0) {
            if (isWouldBlockError()) {
                return false;
            }

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
