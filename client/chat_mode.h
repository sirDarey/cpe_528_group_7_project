#ifndef CHAT_MODE_H
#define CHAT_MODE_H

#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#include "common_utils.h"
#include "client_common.h" // For TCP_PORT, MODE_CHAT, running

// Helper function for chat mode to receive messages
inline void chatReceiver(int sockfd) {
    char buffer[1024];
    while (running) { // Use global 'running' for overall app control
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            logInfo("Chat server disconnected or error.");
            break;
        }
        buffer[bytes] = '\0';
        logInfo(std::string("[Chat] ") + buffer);
    }
}

// Main function for chat mode
inline void runChatMode(const char* server_ip) {
    logInfo("Attempting to connect for Chat...");
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        logError("Failed to create socket for Chat.");
        return;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(TCP_PORT);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        logError("Invalid server IP address for Chat.");
        close(sockfd);
        return;
    }

    if (connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        logError("Failed to connect for Chat: " + std::string(strerror(errno)));
        close(sockfd);
        return;
    }
    logInfo("Connected for Chat.");

    uint8_t mode = MODE_CHAT;
    if (!sendAll(sockfd, (char*)&mode, sizeof(mode))) {
        logError("Failed to send mode to server for Chat.");
        close(sockfd);
        return;
    }

    std::thread receiver(chatReceiver, sockfd);
    
    logInfo("Chat mode started. Type '/exit' to return to main menu.");
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "/exit") {
            logInfo("Exiting chat mode...");
            break;
        }
        if (!sendAll(sockfd, line.c_str(), line.size())) {
            logError("Failed to send message. Connection lost.");
            break;
        }
    }
    
    close(sockfd);
    if (receiver.joinable()) receiver.join();
    logInfo("Chat mode ended.");
}

#endif // CHAT_MODE_H