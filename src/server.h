#ifndef TINYIM_SERVER_H
#define TINYIM_SERVER_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

class AuthService;
class EpollReactor;
class OfflineService;
class SessionManager;

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
};

class Server {
public:
    Server(std::uint16_t port, std::string db_path);
    ~Server();

    bool start();
    void run();

private:
    bool setupListenSocket();
    bool setNonBlocking(int fd);
    void closeListenSocket();
    bool updateClientEvents(int client_fd);
    bool handleAccept();
    void handleClientReadable(int client_fd);
    bool flushClientWrites(int client_fd);
    bool processFrames(int client_fd);
    bool handleApplicationMessage(int client_fd, const std::string& json_text);
    bool sendJsonMessage(int client_fd, const std::string& json_text);
    bool sendJsonObject(int client_fd, const std::map<std::string, std::string>& fields);
    bool deliverOfflineMessages(int client_fd, const std::string& username);
    ConnectionState getConnectionState(int client_fd) const;
    void removeClient(int client_fd);
    bool writeAll(int fd, const char* data, std::size_t length);
    int getMaxFd() const;

    std::uint16_t port_;
    std::string db_path_;
    int listen_fd_;
    std::unique_ptr<EpollReactor> reactor_;
    std::map<int, ClientConnection> clients_;
    std::unique_ptr<AuthService> auth_service_;
    std::unique_ptr<SessionManager> session_manager_;
    std::unique_ptr<OfflineService> offline_service_;
};

#endif
