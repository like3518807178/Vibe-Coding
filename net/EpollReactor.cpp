#include "EpollReactor.h"

#include <sys/epoll.h>
#include <unistd.h>

namespace {

bool ctl(int epoll_fd, int op, int fd, std::uint32_t events) {
    epoll_event event{};
    event.events = events;
    event.data.fd = fd;
    return epoll_ctl(epoll_fd, op, fd, &event) == 0;
}

}  // namespace

EpollReactor::EpollReactor() : epoll_fd_(-1) {}

EpollReactor::~EpollReactor() {
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

bool EpollReactor::create() {
    epoll_fd_ = epoll_create1(0);
    return epoll_fd_ >= 0;
}

bool EpollReactor::add(int fd, std::uint32_t events) {
    return ctl(epoll_fd_, EPOLL_CTL_ADD, fd, events);
}

bool EpollReactor::modify(int fd, std::uint32_t events) {
    return ctl(epoll_fd_, EPOLL_CTL_MOD, fd, events);
}

bool EpollReactor::remove(int fd) {
    return epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

int EpollReactor::wait(std::vector<epoll_event>& events, int timeout_ms) {
    return epoll_wait(epoll_fd_, events.data(), static_cast<int>(events.size()), timeout_ms);
}

int EpollReactor::fd() const {
    return epoll_fd_;
}
