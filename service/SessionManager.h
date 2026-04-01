#ifndef TINYIM_SERVICE_SESSION_MANAGER_H
#define TINYIM_SERVICE_SESSION_MANAGER_H

#include <map>
#include <string>

class SessionManager {
public:
    bool bindUser(const std::string& username, int fd);
    void unbindFd(int fd);
    bool isUserOnline(const std::string& username) const;
    bool isAuthed(int fd) const;
    std::string getUserByFd(int fd) const;
    int getFdByUser(const std::string& username) const;

private:
    std::map<std::string, int> user_to_fd_;
    std::map<int, std::string> fd_to_user_;
};

#endif
