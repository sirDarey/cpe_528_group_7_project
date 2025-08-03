#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

#include <string>
#include <arpa/inet.h> // For inet_ntop, ntohs
#include <sys/socket.h> // For sockaddr_in, getpeername

// Utility: get client IP:port as string
inline std::string getClientInfo(int sockfd) {
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(sockfd, (sockaddr*)&addr, &addr_len);
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(addr.sin_port);
    return std::string(client_ip) + ":" + std::to_string(client_port);
}

#endif // SERVER_UTILS_H