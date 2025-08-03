#ifndef CHAT_HANDLER_H
#define CHAT_HANDLER_H

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm> // For std::remove
#include <unistd.h>  // For close

#include "server_utils.h"
#include "common_utils.h"
#include "server_common.h" // For running, chatMutex, chatClients

inline void handleChatClient(int sockfd) {
    std::string client_info = getClientInfo(sockfd); // getClientInfo from server_utils.h
    char buffer[1024];
    logInfo("Chat client connected: " + client_info);

    while (running) {
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            logInfo("Chat client disconnected: " + client_info);
            close(sockfd);
            {
                std::lock_guard<std::mutex> lock(chatMutex);
                chatClients.erase(std::remove(chatClients.begin(), chatClients.end(), sockfd), chatClients.end());
                logInfo("Chat clients connected: " + std::to_string(chatClients.size()));
            }
            break;
        }
        
        buffer[bytes] = '\0';
        std::string msg = "[Chat][" + client_info + "] " + std::string(buffer);
        logInfo(msg);
        
        std::lock_guard<std::mutex> lock(chatMutex);
        for (int client : chatClients) {
            if (client != sockfd) {
                sendAll(client, buffer, bytes);
            }
        }
    }
}

#endif // CHAT_HANDLER_H