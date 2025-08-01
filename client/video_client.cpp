#include <iostream>
#include <opencv2/opencv.hpp>
#include <arpa/inet.h>
#include <unistd.h>
#include "common_utils.h"

#define PORT 8000

int main(int argc, char* argv[]) {
    if (argc != 2) {
        logError("Usage: ./video_client <server_ip>");
        return EXIT_FAILURE;
    }

    const char* server_ip = argv[1];

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        logError("Failed to create TCP socket.");
        return EXIT_FAILURE;
    }

    sockaddr_in servaddr{};
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        logError("Invalid server IP address.");
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (connect(sockfd, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) < 0) {
        logError("Failed to connect to server.");
        close(sockfd);
        return EXIT_FAILURE;
    }

    cv::VideoCapture cap(0, cv::CAP_AVFOUNDATION);
    if (!cap.isOpened()) {
        logError("Could not open camera. Check permissions and availability.");
        close(sockfd);
        return EXIT_FAILURE;
    }

    logInfo("Connected to video server. Streaming video...");

    cv::Mat frame;
    std::vector<uchar> encoded;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 40};

    while (true) {
        cap >> frame;
        if (frame.empty()) {
            logError("Captured empty frame. Stopping.");
            break;
        }

        if (!cv::imencode(".jpg", frame, encoded, params)) {
            logError("Failed to encode frame.");
            continue;
        }

        uint32_t frame_size = htonl(static_cast<uint32_t>(encoded.size()));

        if (!sendAll(sockfd, reinterpret_cast<char*>(&frame_size), sizeof(frame_size))) {
            logError("Failed to send frame size.");
            break;
        }

        if (!sendAll(sockfd, reinterpret_cast<char*>(encoded.data()), encoded.size())) {
            logError("Failed to send frame data.");
            break;
        }

        logInfo("Sent frame (" + std::to_string(encoded.size()) + " bytes)");

        cv::imshow("Local Camera", frame);
        if (cv::waitKey(10) == 27) break; // ESC to exit
    }

    cap.release();
    close(sockfd);
    logInfo("Streaming stopped.");
    return EXIT_SUCCESS;
}