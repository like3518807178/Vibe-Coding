#include "SessionManager.h"

bool SessionManager::bindUser(const std::string& username, int fd) {
    if (isUserOnline(username)) {
        return false;
    }

    user_to_fd_[username] = fd;
    fd_to_user_[fd] = username;
    return true;
}

void SessionManager::unbindFd(int fd) {
    auto fd_it = fd_to_user_.find(fd);
    if (fd_it == fd_to_user_.end()) {
        return;
    }

    user_to_fd_.erase(fd_it->second);
    fd_to_user_.erase(fd_it);
}

bool SessionManager::isUserOnline(const std::string& username) const {
    return user_to_fd_.find(username) != user_to_fd_.end();
}

bool SessionManager::isAuthed(int fd) const {
    return fd_to_user_.find(fd) != fd_to_user_.end();
}

std::string SessionManager::getUserByFd(int fd) const {
    auto it = fd_to_user_.find(fd);
    if (it == fd_to_user_.end()) {
        return "";
    }

    return it->second;
}

int SessionManager::getFdByUser(const std::string& username) const {
    auto it = user_to_fd_.find(username);
    if (it == user_to_fd_.end()) {
        return -1;
    }

    return it->second;
}
