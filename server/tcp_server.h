#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <iostream>
#include <string>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring> // For strerror

#include "common_utils.h"
#include "server_common.h"
#include "server_utils.h"  // For getClientInfo
#include "chat_handler.h"  // For handleChatClient
#include "file_handler.h"  // For handleFileClient
#include "video_handler.h" // For handleVideoClient

inline void tcpServer() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(TCP_PORT);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    logInfo("TCP server started on port " + std::to_string(TCP_PORT));

    while (running) {
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &addrlen);
        if (client_fd < 0) continue;
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);
        logInfo("New connection from " + std::string(client_ip) + ":" + std::to_string(client_port));
        
        uint8_t mode;
        if (!recvAll(client_fd, (char*)&mode, sizeof(mode))) {
            close(client_fd);
            continue;
        }
        
        if (mode == MODE_CHAT) {
            std::lock_guard<std::mutex> lock(chatMutex);
            if (chatClients.size() >= MAX_CHAT_CLIENTS) {
                logError("Max chat clients reached. Rejecting connection from " +
                        std::string(client_ip) + ":" + std::to_string(client_port));
                close(client_fd);
                continue;
            }
            chatClients.push_back(client_fd);
            logInfo("Chat clients connected: " + std::to_string(chatClients.size()) +
                   "/" + std::to_string(MAX_CHAT_CLIENTS));
            std::thread(handleChatClient, client_fd).detach();
        } else if (mode == MODE_FILE) {
            std::thread(handleFileClient, client_fd).detach();
        } else if (mode == MODE_VIDEO) {
            // Wait for any existing video client to finish
            {
                std::lock_guard<std::mutex> lock(videoClientMutex);
                if (videoClientThread.joinable()) {
                    videoClientConnected = false; // Signal old thread to exit
                    videoClientThread.join();
                }
                videoClientThread = std::thread(handleVideoClient, client_fd);
            }
        } else {
            logError("Unknown mode from " + std::string(client_ip) + ":" + std::to_string(client_port));
            close(client_fd);
        }
    }

    close(server_fd);
}

#endif // TCP_SERVER_H