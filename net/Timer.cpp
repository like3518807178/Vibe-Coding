#include "Timer.h"

#include <sys/timerfd.h>
#include <unistd.h>

Timer::Timer() : timer_fd_(-1) {}

Timer::~Timer() {
    closeFd();
}

bool Timer::create() {
    closeFd();
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    return timer_fd_ >= 0;
}

bool Timer::armPeriodic(int interval_ms) {
    if (timer_fd_ < 0 || interval_ms <= 0) {
        return false;
    }

    const time_t sec = interval_ms / 1000;
    const long nsec = static_cast<long>(interval_ms % 1000) * 1000000L;

    itimerspec spec{};
    spec.it_value.tv_sec = sec;
    spec.it_value.tv_nsec = nsec;
    spec.it_interval.tv_sec = sec;
    spec.it_interval.tv_nsec = nsec;

    return timerfd_settime(timer_fd_, 0, &spec, nullptr) == 0;
}

bool Timer::readExpirations(std::uint64_t& expirations) {
    expirations = 0;
    if (timer_fd_ < 0) {
        return false;
    }

    const ssize_t n = read(timer_fd_, &expirations, sizeof(expirations));
    return n == static_cast<ssize_t>(sizeof(expirations));
}

void Timer::closeFd() {
    if (timer_fd_ >= 0) {
        close(timer_fd_);
        timer_fd_ = -1;
    }
}

int Timer::fd() const {
    return timer_fd_;
}
