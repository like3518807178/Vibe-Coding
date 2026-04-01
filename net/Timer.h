#ifndef TINYIM_NET_TIMER_H
#define TINYIM_NET_TIMER_H

#include <cstdint>

class Timer {
public:
    Timer();
    ~Timer();

    bool create();
    bool armPeriodic(int interval_ms);
    bool readExpirations(std::uint64_t& expirations);
    void closeFd();
    int fd() const;

private:
    int timer_fd_;
};

#endif
