#include <iostream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "common_utils.h"

#define PORT 5000
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

int main() {
    int server_fd, new_socket, client_socket[MAX_CLIENTS];
    sockaddr_in address{};
    char buffer[BUFFER_SIZE + 1];
    fd_set readfds;

    std::fill_n(client_socket, MAX_CLIENTS, 0);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        logError("Socket creation failed.");
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logError("setsockopt failed.");
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

    logInfo("Chat server listening on port " + std::to_string(PORT));

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_socket[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        int activity = select(max_sd + 1, &readfds, nullptr, nullptr, nullptr);
        if (activity < 0) {
            logError("select() failed.");
            continue;
        }

        if (FD_ISSET(server_fd, &readfds)) {
            socklen_t addrlen = sizeof(address);
            new_socket = accept(server_fd, reinterpret_cast<sockaddr*>(&address), &addrlen);
            if (new_socket < 0) {
                logError("accept() failed.");
                continue;
            }

            logInfo("New connection, socket fd: " + std::to_string(new_socket));

            bool added = false;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    added = true;
                    break;
                }
            }

            if (!added) {
                logError("Max clients reached. Connection rejected.");
                close(new_socket);
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_socket[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                ssize_t valread = read(sd, buffer, BUFFER_SIZE);
                if (valread <= 0) {
                    getpeername(sd, reinterpret_cast<sockaddr*>(&address), reinterpret_cast<socklen_t*>(&address));
                    logInfo("Client disconnected, socket: " + std::to_string(sd));
                    close(sd);
                    client_socket[i] = 0;
                } else {
                    buffer[valread] = '\0';
                    std::string msg = "[Client " + std::to_string(sd) + "]: " + buffer;
                    logInfo(msg);

                    // Broadcast to other clients
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (client_socket[j] > 0 && client_socket[j] != sd) {
                            if (!sendAll(client_socket[j], buffer, strlen(buffer))) {
                                logError("Failed to send to client " + std::to_string(client_socket[j]));
                            }
                        }
                    }
                }
            }
        }
    }

    close(server_fd);
    return EXIT_SUCCESS;
}