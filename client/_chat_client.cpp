#include <iostream>
#include <thread>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common_utils.h"

#define PORT 5000
#define BUFFER_SIZE 1024

volatile bool running = true;

void handleSignal(int) {
    running = false;
    std::cout << "\n[Client] Shutting down gracefully...\n";
}

void receiveMessages(int sock) {
    char buffer[BUFFER_SIZE + 1];
    while (running) {
        ssize_t valread = recv(sock, buffer, BUFFER_SIZE, 0);
        if (valread <= 0) {
            logError("Disconnected from server.");
            break;
        }
        buffer[valread] = '\0';
        logInfo(std::string("[Message] ") + buffer);
    }
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        logError("Usage: ./chat_client <server_ip>");
        return EXIT_FAILURE;
    }

    signal(SIGINT, handleSignal);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logError("Socket creation failed.");
        return EXIT_FAILURE;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        logError("Invalid IP address.");
        close(sock);
        return EXIT_FAILURE;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        logError("Connection to server failed.");
        close(sock);
        return EXIT_FAILURE;
    }

    logInfo("Connected to chat server. Type messages and press Enter to send.");

    std::thread receiver(receiveMessages, sock);

    char buffer[BUFFER_SIZE];
    while (running && std::cin.getline(buffer, BUFFER_SIZE)) {
        if (!sendAll(sock, buffer, strlen(buffer))) {
            logError("Failed to send message.");
            break;
        }
    }

    running = false;
    shutdown(sock, SHUT_RDWR);
    close(sock);
    if (receiver.joinable()) receiver.join();

    logInfo("Client exited.");
    return EXIT_SUCCESS;
}