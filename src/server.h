#ifndef TINYIM_SERVER_H
#define TINYIM_SERVER_H

#include <cstdint>

class Server {
public:
    explicit Server(std::uint16_t port);
    ~Server();

    bool start();
    void run();

private:
    bool setupListenSocket();
    void closeListenSocket();
    void handleClient(int client_fd);
    bool writeAll(int fd, const char* data, int length);

    std::uint16_t port_;
    int listen_fd_;
};

#endif
