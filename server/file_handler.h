#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <unistd.h> // For close
#include <algorithm> // For std::min
#include <cstring> // For strerror

#include "common_utils.h"
#include "server_utils.h"
#include "server_common.h" // For BUFFER_SIZE

inline void handleFileClient(int sockfd) {
    std::string client_info = getClientInfo(sockfd); // getClientInfo from server_utils.h
    uint32_t name_len_net;
    if (!recvAll(sockfd, reinterpret_cast<char*>(&name_len_net), sizeof(name_len_net))) {
        logError("Failed to read filename length from " + client_info);
        close(sockfd);
        return;
    }
    
    uint32_t name_len = ntohl(name_len_net);
    if (name_len == 0 || name_len >= 256) {
        logError("Invalid filename length from " + client_info);
        close(sockfd);
        return;
    }
    
    char filename[256] = {0};
    if (!recvAll(sockfd, filename, name_len)) {
        logError("Failed to read filename from " + client_info);
        close(sockfd);
        return;
    }
    filename[name_len] = '\0';
    
    uint64_t file_size_net;
    if (!recvAll(sockfd, reinterpret_cast<char*>(&file_size_net), sizeof(file_size_net))) {
        logError("Failed to read file size from " + client_info);
        close(sockfd);
        return;
    }
    
    uint64_t file_size = ntohll(file_size_net);
    logInfo("File transfer started from " + client_info + ": " + std::string(filename) +
            " (" + std::to_string(file_size) + " bytes)");
    
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile.is_open()) {
        logError("Failed to create file: " + std::string(filename));
        close(sockfd);
        return;
    }
    
    char buffer[BUFFER_SIZE];
    uint64_t received = 0;
    while (received < file_size) {
        size_t to_read = std::min(static_cast<uint64_t>(BUFFER_SIZE), file_size - received);
        ssize_t bytes = recv(sockfd, buffer, to_read, 0);
        if (bytes <= 0) break;
        outFile.write(buffer, bytes);
        received += bytes;
    }
    
    outFile.close();
    close(sockfd);
    
    if (received == file_size) logInfo("File received successfully from " + client_info);
    else logError("File transfer incomplete from " + client_info);
}

#endif // FILE_HANDLER_H