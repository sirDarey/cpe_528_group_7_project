#ifndef FILE_MODE_H
#define FILE_MODE_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <algorithm> // For std::min

#include "common_utils.h"
#include "client_common.h" // For TCP_PORT, MODE_FILE, BUFFER_SIZE

// Main function for file transfer mode
inline void runFileMode(const char* server_ip) {
    std::string file_path;
    std::cout << "Enter file path to send: ";
    std::getline(std::cin, file_path);

    if (file_path.empty()) {
        logInfo("No file selected. Returning to main menu.");
        return;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        logError("Error opening file: " + file_path);
        return;
    }

    std::string filename = file_path.substr(file_path.find_last_of("/\\") + 1);
    file.seekg(0, std::ios::end);
    uint64_t file_size = file.tellg();
    file.seekg(0);

    logInfo("Attempting to connect for File transfer...");
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        logError("Failed to create socket for File transfer.");
        file.close();
        return;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(TCP_PORT);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        logError("Invalid server IP address for File transfer.");
        close(sockfd);
        file.close();
        return;
    }

    if (connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        logError("Failed to connect for File transfer: " + std::string(strerror(errno)));
        close(sockfd);
        file.close();
        return;
    }
    logInfo("Connected for File transfer.");

    uint8_t mode = MODE_FILE;
    if (!sendAll(sockfd, (char*)&mode, sizeof(mode))) {
        logError("Failed to send mode to server for File transfer.");
        close(sockfd);
        file.close();
        return;
    }

    uint32_t name_len_net = htonl((uint32_t)filename.size());
    sendAll(sockfd, (char*)&name_len_net, sizeof(name_len_net));
    sendAll(sockfd, filename.c_str(), filename.size());

    uint64_t size_net = htonll(file_size);
    sendAll(sockfd, (char*)&size_net, sizeof(size_net));

    logInfo("Sending file: " + filename + " (" + std::to_string(file_size) + " bytes)");

    char buffer[BUFFER_SIZE];
    uint64_t sent = 0;
    while (file && sent < file_size) {
        file.read(buffer, BUFFER_SIZE);
        std::streamsize bytes = file.gcount();
        if (bytes > 0) {
            if (!sendAll(sockfd, buffer, bytes)) {
                logError("Failed to send file data. Connection lost.");
                break;
            }
            sent += bytes;
            int progress = (int)((sent * 100) / file_size);
            std::cout << "\rProgress: " << progress << "% (" << sent << "/" << file_size << " bytes)" << std::flush;
        }
    }

    file.close();
    close(sockfd);
    std::cout << std::endl; // Newline after progress bar

    if (sent == file_size) {
        logInfo("File sent successfully.");
    } else {
        logError("File transfer incomplete.");
    }
}

#endif // FILE_MODE_H