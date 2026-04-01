#include "server.h"

#include "../common/Logger.h"
#include "../net/EpollReactor.h"
#include "../net/Timer.h"
#include "../protocol/FramingCodec.h"
#include "../protocol/JsonCodec.h"
#include "../service/AuthService.h"
#include "../service/OfflineService.h"
#include "../service/SessionManager.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr int kBacklog = 16;
constexpr std::size_t kBufferSize = 4096;
constexpr int kMaxEvents = 64;
constexpr int kTimerIntervalMs = 1000;

void logErrno(const std::string& action) {
    Logger::error(action + " failed: " + std::strerror(errno));
}

bool isWouldBlockError() {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

std::uint64_t currentTimeMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

}  // namespace

Server::Server(AppConfig config)
    : port_(config.port),
      db_path_(std::move(config.db_path)),
      max_packet_size_(config.max_packet_size),
      idle_timeout_seconds_(config.idle_timeout),
      listen_fd_(-1),
      timer_fd_(-1),
      reactor_(std::make_unique<EpollReactor>()),
      auth_service_(std::make_unique<AuthService>(db_path_)),
      session_manager_(std::make_unique<SessionManager>()),
      offline_service_(std::make_unique<OfflineService>(db_path_, *session_manager_)),
      timer_(std::make_unique<Timer>()) {}

Server::~Server() {
    for (const auto& entry : clients_) {
        if (close(entry.first) < 0) {
            logErrno("close client socket");
        }
    }

    closeListenSocket();
    closeTimer();
}

bool Server::start() {
    std::string db_error;
    if (!auth_service_->init(db_error)) {
        Logger::error("Auth service init failed: " + db_error);
        return false;
    }

    if (!offline_service_->init(db_error)) {
        Logger::error("Offline service init failed: " + db_error);
        return false;
    }

    if (!reactor_->create()) {
        logErrno("epoll_create1");
        return false;
    }

    if (!setupTimer()) {
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

            logErrno("epoll_wait");
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

            if (fd == timer_fd_) {
                if (!handleTimerReadable()) {
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
        logErrno("socket");
        return false;
    }

    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logErrno("setsockopt SO_REUSEADDR");
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
        logErrno("bind");
        closeListenSocket();
        return false;
    }

    if (listen(listen_fd_, kBacklog) < 0) {
        logErrno("listen");
        closeListenSocket();
        return false;
    }

    if (!reactor_->add(listen_fd_, EPOLLIN)) {
        logErrno("epoll_ctl add listen fd");
        closeListenSocket();
        return false;
    }

    Logger::info("Server listening on 0.0.0.0:" + std::to_string(port_));
    return true;
}

bool Server::setupTimer() {
    if (!timer_->create()) {
        logErrno("timerfd_create");
        return false;
    }

    if (!timer_->armPeriodic(kTimerIntervalMs)) {
        logErrno("timerfd_settime");
        return false;
    }

    timer_fd_ = timer_->fd();
    if (!reactor_->add(timer_fd_, EPOLLIN)) {
        logErrno("epoll_ctl add timer fd");
        closeTimer();
        return false;
    }

    return true;
}

bool Server::setNonBlocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        logErrno("fcntl F_GETFL");
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        logErrno("fcntl F_SETFL O_NONBLOCK");
        return false;
    }

    return true;
}

void Server::closeListenSocket() {
    if (listen_fd_ >= 0) {
        if (close(listen_fd_) < 0) {
            logErrno("close listen socket");
        }
        listen_fd_ = -1;
    }
}

void Server::closeTimer() {
    if (timer_) {
        timer_->closeFd();
    }
    timer_fd_ = -1;
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
        logErrno("epoll_ctl mod client fd");
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

            logErrno("accept");
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

        clients_.emplace(
            client_fd,
            ClientConnection{
                client_fd,
                "",
                "",
                ConnectionState::Connected,
                currentTimeMs()
            }
        );
        if (!reactor_->add(client_fd, EPOLLIN)) {
            logErrno("epoll_ctl add client fd");
            clients_.erase(client_fd);
            close(client_fd);
            continue;
        }

        Logger::info(
            "Client connected: " + ip_text + ":" + std::to_string(ntohs(client_addr.sin_port)) +
            ", fd=" + std::to_string(client_fd)
        );
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
            Logger::info("Client disconnected: fd=" + std::to_string(client_fd));
            removeClient(client_fd);
            return;
        }

        if (bytes_read < 0) {
            if (isWouldBlockError()) {
                return;
            }

            logErrno("read");
            removeClient(client_fd);
            return;
        }

        it = clients_.find(client_fd);
        if (it == clients_.end()) {
            return;
        }

        refreshLastActive(client_fd);
        it->second.inbuf.append(buffer, static_cast<std::size_t>(bytes_read));
        if (it->second.inbuf.size() > static_cast<std::size_t>(max_packet_size_) + sizeof(std::uint32_t)) {
            Logger::error("Client sent oversized buffered data: fd=" + std::to_string(client_fd));
            removeClient(client_fd);
            return;
        }

        if (!processFrames(client_fd)) {
            removeClient(client_fd);
            return;
        }
    }
}

bool Server::handleTimerReadable() {
    std::uint64_t expirations = 0;
    if (!timer_->readExpirations(expirations)) {
        logErrno("read timerfd");
        return false;
    }

    const std::vector<int> timeout_fds = collectIdleTimeoutFds();
    for (int fd : timeout_fds) {
        Logger::info("Idle timeout, closing fd=" + std::to_string(fd));
        removeClient(fd);
    }

    return true;
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

            logErrno("write");
            return false;
        }

        if (bytes_written == 0) {
            Logger::error("write failed: wrote 0 bytes");
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
            Logger::error("Invalid frame from fd=" + std::to_string(client_fd) + ": " + frame_error);
            return false;
        }

        if (!got_frame) {
            return true;
        }

        std::map<std::string, std::string> fields;
        std::string json_error;
        if (!protocol::JsonCodec::parseObject(frame, fields, json_error)) {
            Logger::error(
                "JSON body rejected by protocol from fd=" + std::to_string(client_fd) +
                ": " + json_error
            );
            return false;
        }

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
        Logger::error(
            "Failed to parse application JSON from fd=" + std::to_string(client_fd) +
            ": " + parse_error
        );
        return false;
    }

    const auto type_it = fields.find("type");
    if (type_it != fields.end() && type_it->second == "heartbeat") {
        refreshLastActive(client_fd);
        Logger::info("Heartbeat received: fd=" + std::to_string(client_fd));
        return sendJsonObject(
            client_fd,
            {
                {"type", "heartbeat_resp"},
                {"message", "pong"},
                {"ok", "true"}
            }
        );
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
                Logger::info("Login rejected, user already online: user=" + auth_result.username);
            } else if (!session_manager_->bindUser(auth_result.username, client_fd)) {
                response["ok"] = "false";
                response["message"] = "绑定在线态失败";
                Logger::error("Bind session failed: user=" + auth_result.username +
                              ", fd=" + std::to_string(client_fd));
            } else {
                auto it = clients_.find(client_fd);
                if (it != clients_.end()) {
                    it->second.state = ConnectionState::Authed;
                }
                Logger::info("Login success: user=" + auth_result.username +
                             ", fd=" + std::to_string(client_fd));
            }
        } else if (auth_result.type == "login_resp") {
            Logger::info("Login failed: user=" + auth_result.username +
                         ", reason=" + auth_result.message);
        } else if (auth_result.type == "register_resp" && auth_result.success) {
            Logger::info("Register success: user=" + auth_result.username);
        } else if (auth_result.type == "register_resp") {
            Logger::info("Register failed: user=" + auth_result.username +
                         ", reason=" + auth_result.message);
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
        Logger::error("Send failed: from=" + from_user + ", to=" + to_it->second +
                      ", reason=" + (offline_error.empty() ? "发送失败" : offline_error));
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
        Logger::info("Send route online: from=" + from_user + ", to=" + to_it->second +
                     ", target_fd=" + std::to_string(dispatch.target_fd));
        if (!sendJsonObject(dispatch.target_fd, dispatch.deliver_payload)) {
            return false;
        }
    } else if (dispatch.stored_offline) {
        Logger::info("Send route offline store: from=" + from_user + ", to=" + to_it->second);
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
                logErrno("write");
                return false;
            }

            it->second.outbuf = packet;
            return updateClientEvents(client_fd);
        }

        if (bytes_written == 0) {
            Logger::error("write failed: wrote 0 bytes");
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
        Logger::error("Failed to list offline messages for " + username + ": " + error);
        return false;
    }

    for (const auto& record : records) {
        Logger::info(
            "Deliver offline message: user=" + username +
            ", msg_id=" + std::to_string(record.msg_id)
        );
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
            Logger::error(
                "Failed to mark offline message delivered, msg_id=" +
                std::to_string(record.msg_id) + ": " + error
            );
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

void Server::refreshLastActive(int client_fd) {
    auto it = clients_.find(client_fd);
    if (it == clients_.end()) {
        return;
    }

    it->second.last_active_ms = currentTimeMs();
}

std::vector<int> Server::collectIdleTimeoutFds() const {
    std::vector<int> timeout_fds;
    const std::uint64_t now_ms = currentTimeMs();
    const std::uint64_t idle_timeout_ms = static_cast<std::uint64_t>(idle_timeout_seconds_) * 1000ULL;

    for (const auto& entry : clients_) {
        const ClientConnection& client = entry.second;
        if (now_ms > client.last_active_ms &&
            now_ms - client.last_active_ms > idle_timeout_ms) {
            timeout_fds.push_back(entry.first);
        }
    }

    return timeout_fds;
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
        logErrno("close client socket");
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

            logErrno("write");
            return false;
        }

        if (bytes_written == 0) {
            Logger::error("write failed: wrote 0 bytes");
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
