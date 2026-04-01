#ifndef TINYIM_SERVER_H
#define TINYIM_SERVER_H

#include "../common/Config.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <utility>

class AuthService;
class EpollReactor;
class OfflineService;
class SessionManager;
class Timer;

enum class ConnectionState {
    Connected,
    Authed,
    Closed
};

struct ClientConnection {
    int fd;
    std::string inbuf;
    std::string outbuf;
    ConnectionState state;
    std::uint64_t last_active_ms;
};

class Server {
public:
    explicit Server(AppConfig config);
    ~Server();

    bool start();
    void run();

private:
    bool setupListenSocket();
    bool setupTimer();
    bool setNonBlocking(int fd);
    void closeListenSocket();
    void closeTimer();
    bool updateClientEvents(int client_fd);
    bool handleAccept();
    void handleClientReadable(int client_fd);
    bool handleTimerReadable();
    bool flushClientWrites(int client_fd);
    bool processFrames(int client_fd);
    bool handleApplicationMessage(int client_fd, const std::string& json_text);
    bool sendJsonMessage(int client_fd, const std::string& json_text);
    bool sendJsonObject(int client_fd, const std::map<std::string, std::string>& fields);
    bool deliverOfflineMessages(int client_fd, const std::string& username);
    ConnectionState getConnectionState(int client_fd) const;
    void refreshLastActive(int client_fd);
    std::vector<int> collectIdleTimeoutFds() const;
    void removeClient(int client_fd);
    bool writeAll(int fd, const char* data, std::size_t length);
    int getMaxFd() const;

    std::uint16_t port_;
    std::string db_path_;
    std::uint32_t max_packet_size_;
    int idle_timeout_seconds_;
    int listen_fd_;
    int timer_fd_;
    std::unique_ptr<EpollReactor> reactor_;
    std::map<int, ClientConnection> clients_;
    std::unique_ptr<AuthService> auth_service_;
    std::unique_ptr<SessionManager> session_manager_;
    std::unique_ptr<OfflineService> offline_service_;
    std::unique_ptr<Timer> timer_;
};

#endif
