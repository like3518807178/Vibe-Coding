#ifndef TINYIM_NET_EPOLL_REACTOR_H
#define TINYIM_NET_EPOLL_REACTOR_H

#include <cstdint>
#include <vector>

struct epoll_event;

class EpollReactor {
public:
    EpollReactor();
    ~EpollReactor();

    bool create();
    bool add(int fd, std::uint32_t events);
    bool modify(int fd, std::uint32_t events);
    bool remove(int fd);
    int wait(std::vector<epoll_event>& events, int timeout_ms);
    int fd() const;

private:
    int epoll_fd_;
};

#endif
