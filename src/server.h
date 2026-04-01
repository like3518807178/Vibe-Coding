#ifndef TINYIM_SERVER_H
#define TINYIM_SERVER_H

#include <cstddef>
#include <cstdint>
#include <set>

class Server {
public:
    explicit Server(std::uint16_t port);
    ~Server();

    bool start();
    void run();

private:
    bool setupListenSocket();
    void closeListenSocket();
    bool handleNewConnection();
    void handleClientMessage(int client_fd);
    void removeClient(int client_fd);
    void broadcastMessage(int sender_fd, const char* data, int length);
    bool writeAll(int fd, const char* data, std::size_t length);
    int getMaxFd() const;

    std::uint16_t port_;
    int listen_fd_;
    std::set<int> client_fds_;
};

#endif
