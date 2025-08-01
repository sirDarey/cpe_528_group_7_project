#include <iostream>
#include <opencv2/opencv.hpp>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include "common_utils.h"

#define PORT 8000

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        logError("Failed to create TCP socket.");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        logError("Bind failed.");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 1) < 0) {
        logError("Listen failed.");
        close(server_fd);
        return EXIT_FAILURE;
    }

    logInfo("Video server listening on port " + std::to_string(PORT));

    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
    if (client_fd < 0) {
        logError("Accept failed.");
        close(server_fd);
        return EXIT_FAILURE;
    }

    logInfo("Client connected.");

    while (true) {
        uint32_t frame_size_net;
        if (!recvAll(client_fd, reinterpret_cast<char*>(&frame_size_net), sizeof(frame_size_net))) {
            logError("Failed to receive frame size.");
            break;
        }

        uint32_t frame_size = ntohl(frame_size_net);
        std::vector<uchar> buffer(frame_size);

        if (!recvAll(client_fd, reinterpret_cast<char*>(buffer.data()), frame_size)) {
            logError("Failed to receive frame data.");
            break;
        }

        cv::Mat frame = cv::imdecode(buffer, cv::IMREAD_COLOR);
        if (!frame.empty()) {
            cv::imshow("Live Video Feed", frame);
            if (cv::waitKey(1) == 27) break; // ESC to quit
        } else {
            logError("Failed to decode frame.");
        }
    }

    close(client_fd);
    close(server_fd);
    logInfo("Video server stopped.");
    return EXIT_SUCCESS;
}