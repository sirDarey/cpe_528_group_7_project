#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <algorithm>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <portaudio.h>
#include "common_utils.h"

#define TCP_PORT 5000
#define UDP_VOICE_PORT 7000
#define BUFFER_SIZE 4096
#define MAX_CHAT_CLIENTS 50

// Mode IDs
#define MODE_CHAT  1
#define MODE_FILE  2
#define MODE_VIDEO 3

std::vector<int> chatClients;
std::mutex chatMutex;
volatile bool running = true;
volatile bool videoStreaming = true; // Separate flag for video display

// Thread-safe queue for video frames
std::queue<cv::Mat> videoFrameQueue;
std::mutex videoQueueMutex;
std::condition_variable videoCond;

// Utility: get client IP:port as string
std::string getClientInfo(int sockfd) {
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(sockfd, (sockaddr*)&addr, &addr_len);
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(addr.sin_port);
    return std::string(client_ip) + ":" + std::to_string(client_port);
}

void handleChatClient(int sockfd) {
    std::string client_info = getClientInfo(sockfd);
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

void handleFileClient(int sockfd) {
    std::string client_info = getClientInfo(sockfd);

    uint32_t name_len_net;
    if (!recvAll(sockfd, reinterpret_cast<char*>(&name_len_net), sizeof(name_len_net))) {
        logError("Failed to read filename length from " + client_info);
        close(sockfd);
        return;
    }
    uint32_t name_len = ntohl(name_len_net);
    if (name_len == 0 || name_len >= 256) {
        logError("Invalid filename length from " + client_info);
        close(sockfd);
        return;
    }
    char filename[256] = {0};
    if (!recvAll(sockfd, filename, name_len)) {
        logError("Failed to read filename from " + client_info);
        close(sockfd);
        return;
    }
    filename[name_len] = '\0';
    uint64_t file_size_net;
    if (!recvAll(sockfd, reinterpret_cast<char*>(&file_size_net), sizeof(file_size_net))) {
        logError("Failed to read file size from " + client_info);
        close(sockfd);
        return;
    }
    uint64_t file_size = ntohll(file_size_net);

    logInfo("File transfer started from " + client_info + ": " + std::string(filename) +
            " (" + std::to_string(file_size) + " bytes)");

    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile.is_open()) {
        logError("Failed to create file: " + std::string(filename));
        close(sockfd);
        return;
    }
    char buffer[BUFFER_SIZE];
    uint64_t received = 0;
    while (received < file_size) {
        size_t to_read = std::min(static_cast<uint64_t>(BUFFER_SIZE), file_size - received);
        ssize_t bytes = recv(sockfd, buffer, to_read, 0);
        if (bytes <= 0) break;
        outFile.write(buffer, bytes);
        received += bytes;
    }
    outFile.close();
    close(sockfd);
    if (received == file_size) logInfo("File received successfully from " + client_info);
    else logError("File transfer incomplete from " + client_info);
}

void handleVideoClient(int sockfd) {
    std::string client_info = getClientInfo(sockfd);
    logInfo("Video streaming started from " + client_info);
    videoStreaming = true;

    while (running) {
        uint32_t frame_size_net;
        if (!recvAll(sockfd, reinterpret_cast<char*>(&frame_size_net), sizeof(frame_size_net))) break;
        uint32_t frame_size = ntohl(frame_size_net);

        if (frame_size == 0) {
            logInfo("End of video stream from " + client_info);
            videoStreaming = false;
            videoCond.notify_one();
            break;
        }
        
        std::vector<uchar> buffer(frame_size);
        if (!recvAll(sockfd, reinterpret_cast<char*>(buffer.data()), frame_size)) break;
        cv::Mat frame = cv::imdecode(buffer, cv::IMREAD_COLOR);
        if (!frame.empty()) {
            {
                std::lock_guard<std::mutex> lock(videoQueueMutex);
                videoFrameQueue.push(frame);
            }
            videoCond.notify_one();
        }
    }
    close(sockfd);
    logInfo("Video streaming ended from " + client_info);
}

void videoDisplayLoop() {
    while (videoStreaming) {
        std::unique_lock<std::mutex> lock(videoQueueMutex);

        // Wait max 30ms so OpenCV can still process events
        videoCond.wait_for(lock, std::chrono::milliseconds(30), [] { 
            return !videoFrameQueue.empty() || !videoStreaming; 
        });

        if (!videoStreaming) break;

        if (!videoFrameQueue.empty()) {
            cv::Mat frame = videoFrameQueue.front();
            videoFrameQueue.pop();
            lock.unlock();

            cv::imshow("Live Video Feed", frame);
        }

        int key = cv::waitKey(1);
        if (key == 27) { // ESC in server
            videoStreaming = false;
            break;
        }
    }
    cv::destroyWindow("Live Video Feed");
    logInfo("Video display closed.");
}

void voiceUDPServer() {
    if (Pa_Initialize() != paNoError) {
        logError("Failed to init PortAudio.");
        return;
    }
    PaStream* stream;
    if (Pa_OpenDefaultStream(&stream, 0, 1, paInt16, 44100, 512, nullptr, nullptr) != paNoError) {
        logError("Failed to open PortAudio output.");
        Pa_Terminate();
        return;
    }
    Pa_StartStream(stream);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr{}, cliaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(UDP_VOICE_PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    bind(sockfd, (sockaddr*)&servaddr, sizeof(servaddr));

    char buffer[512 * 2];
    socklen_t len = sizeof(cliaddr);
    logInfo("Voice UDP server started.");
    while (running) {
        ssize_t bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&cliaddr, &len);
        if (bytes > 0) {
            Pa_WriteStream(stream, buffer, 512);
        }
    }
    close(sockfd);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}

void tcpServer() {
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
            std::thread(handleVideoClient, client_fd).detach();
        } else {
            logError("Unknown mode from " + std::string(client_ip) + ":" + std::to_string(client_port));
            close(client_fd);
        }
    }
    close(server_fd);
}

int main() {
    std::thread udpThread(voiceUDPServer);
    std::thread tcpThread(tcpServer);

    videoDisplayLoop();

    if (tcpThread.joinable()) tcpThread.join();
    if (udpThread.joinable()) udpThread.join();

    return 0;
}