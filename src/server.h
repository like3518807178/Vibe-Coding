#ifndef TINYIM_SERVER_H
#define TINYIM_SERVER_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

struct ClientConnection {
    int fd;
    std::string inbuf;
};

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
    void handleClientReadable(int client_fd);
    bool processFrames(int client_fd);
    void removeClient(int client_fd);
    void broadcastJsonMessage(int sender_fd, const std::string& json_text);
    bool writeAll(int fd, const char* data, std::size_t length);
    int getMaxFd() const;

    std::uint16_t port_;
    int listen_fd_;
    std::map<int, ClientConnection> clients_;
};

#endif
