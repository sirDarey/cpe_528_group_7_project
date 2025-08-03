#include <iostream>
#include <fstream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <sys/stat.h>
#include "common_utils.h"

#define PORT 6000
#define BUFFER_SIZE 4096

uint64_t getFileSize(const std::string& path) {
    struct stat stat_buf;
    if (stat(path.c_str(), &stat_buf) != 0) {
        return 0;
    }
    return stat_buf.st_size;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        logError("Usage: ./file_client <server_ip> <file_path>");
        return EXIT_FAILURE;
    }

    const char* server_ip = argv[1];
    std::string file_path = argv[2];
    std::ifstream file(file_path, std::ios::binary);

    if (!file.is_open()) {
        logError("Error opening file: " + file_path);
        return EXIT_FAILURE;
    }

    std::string filename = file_path.substr(file_path.find_last_of("/\\") + 1);
    uint64_t file_size = getFileSize(file_path);

    if (file_size == 0) {
        logError("File is empty or inaccessible.");
        file.close();
        return EXIT_FAILURE;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logError("Socket creation failed.");
        file.close();
        return EXIT_FAILURE;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        logError("Invalid server IP address.");
        file.close();
        close(sock);
        return EXIT_FAILURE;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        logError("Connection to server failed.");
        file.close();
        close(sock);
        return EXIT_FAILURE;
    }

    logInfo("Sending file: " + filename + " (" + std::to_string(file_size) + " bytes)");

    bool success = true;

    // Send filename length
    uint32_t name_len = htonl(static_cast<uint32_t>(filename.size()));
    if (!sendAll(sock, reinterpret_cast<char*>(&name_len), sizeof(name_len))) {
        logError("Failed to send filename length.");
        success = false;
    }

    // Send filename
    if (success && !sendAll(sock, filename.c_str(), filename.size())) {
        logError("Failed to send filename.");
        success = false;
    }

    // Send file size
    uint64_t net_file_size = htonll(file_size);
    if (success && !sendAll(sock, reinterpret_cast<char*>(&net_file_size), sizeof(net_file_size))) {
        logError("Failed to send file size.");
        success = false;
    }

    // Send file content
    if (success) {
        char buffer[BUFFER_SIZE];
        while (file) {
            file.read(buffer, BUFFER_SIZE);
            std::streamsize bytes_read = file.gcount();
            if (bytes_read > 0 && !sendAll(sock, buffer, bytes_read)) {
                logError("Failed to send file data.");
                success = false;
                break;
            }
        }
    }

    if (success) {
        logInfo("File sent successfully!");
    }

    file.close();
    close(sock);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}