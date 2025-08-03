#include "common_utils.h"
#include <cstring>
#include <unistd.h>  // for read/write/close on POSIX

bool sendAll(int sockfd, const char* data, size_t len) {
    size_t totalSent = 0;
    while (totalSent < len) {
        ssize_t sent = send(sockfd, data + totalSent, len - totalSent, 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }
    return true;
}

bool recvAll(int sockfd, char* buffer, size_t len) {
    size_t totalReceived = 0;
    while (totalReceived < len) {
        ssize_t received = recv(sockfd, buffer + totalReceived, len - totalReceived, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
}

// Logging
void logInfo(const std::string& msg) {
    std::cout << "[INFO] " << msg << std::endl;
}

void logError(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}