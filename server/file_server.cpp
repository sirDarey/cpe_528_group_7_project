#include <iostream>
#include <fstream>
#include <unistd.h>
#include <netinet/in.h>
#include <cstring>
#include "common_utils.h"

#define PORT 6000
#define BUFFER_SIZE 4096

int main() {
    int server_fd, new_socket;
    sockaddr_in address{};
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        logError("Socket creation failed.");
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logError("setsockopt() failed.");
        close(server_fd);
        return EXIT_FAILURE;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        logError("Bind failed.");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 3) < 0) {
        logError("Listen failed.");
        close(server_fd);
        return EXIT_FAILURE;
    }

    logInfo("File server listening on port " + std::to_string(PORT));

    while (true) {
        new_socket = accept(server_fd, reinterpret_cast<sockaddr*>(&address), &addrlen);
        if (new_socket < 0) {
            logError("Accept failed.");
            continue;
        }

        // Receive filename size
        uint32_t name_len = 0;
        if (!recvAll(new_socket, reinterpret_cast<char*>(&name_len), sizeof(name_len))) {
            logError("Failed to receive filename length.");
            close(new_socket);
            continue;
        }
        name_len = ntohl(name_len);

        if (name_len == 0 || name_len >= 256) {
            logError("Invalid filename length.");
            close(new_socket);
            continue;
        }

        // Receive filename
        char filename[256] = {0};
        if (!recvAll(new_socket, filename, name_len)) {
            logError("Failed to receive filename.");
            close(new_socket);
            continue;
        }
        filename[name_len] = '\0';

        // Receive file size
        uint64_t file_size = 0;
        if (!recvAll(new_socket, reinterpret_cast<char*>(&file_size), sizeof(file_size))) {
            logError("Failed to receive file size.");
            close(new_socket);
            continue;
        }
        file_size = ntohll(file_size);

        std::ofstream outFile(filename, std::ios::binary);
        if (!outFile.is_open()) {
            logError("Failed to open file for writing: " + std::string(filename));
            close(new_socket);
            continue;
        }

        logInfo("Receiving file: " + std::string(filename) + " (" + std::to_string(file_size) + " bytes)");

        uint64_t received = 0;
        while (received < file_size) {
            size_t to_read = std::min(static_cast<uint64_t>(BUFFER_SIZE), file_size - received);
            ssize_t bytes = read(new_socket, buffer, to_read);
            if (bytes <= 0) {
                logError("Connection lost during file transfer.");
                break;
            }
            outFile.write(buffer, bytes);
            received += bytes;
        }

        outFile.close();
        close(new_socket);

        if (received == file_size) {
            logInfo("File received successfully.");
        } else {
            logError("File transfer incomplete. Received " + std::to_string(received) + " of " + std::to_string(file_size) + " bytes.");
        }
    }

    close(server_fd);
    return EXIT_SUCCESS;
}