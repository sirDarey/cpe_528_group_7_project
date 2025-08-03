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

// Video streaming control with better synchronization
std::atomic<bool> videoStreaming{false};
std::atomic<bool> videoClientConnected{false};
std::atomic<bool> shouldCloseWindow{false};
std::atomic<bool> videoSessionActive{false};

// Thread-safe queue for video frames
std::queue<cv::Mat> videoFrameQueue;
std::mutex videoQueueMutex;
std::condition_variable videoCond;

// Video client thread tracking
std::thread videoClientThread;
std::mutex videoClientMutex;

// Main thread video control
std::condition_variable mainThreadCond;
std::mutex mainThreadMutex;

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
    
    // Set flags to indicate video client is active
    videoClientConnected = true;
    videoStreaming = true;
    videoSessionActive = true;
    shouldCloseWindow = false;
    
    // Notify main thread to start video display
    mainThreadCond.notify_one();
    
    while (running && videoClientConnected) {
        uint32_t frame_size_net;
        if (!recvAll(sockfd, reinterpret_cast<char*>(&frame_size_net), sizeof(frame_size_net))) {
            break;
        }
        
        uint32_t frame_size = ntohl(frame_size_net);
        if (frame_size == 0) {
            logInfo("End of video stream from " + client_info);
            break;
        }
        
        std::vector<uchar> buffer(frame_size);
        if (!recvAll(sockfd, reinterpret_cast<char*>(buffer.data()), frame_size)) {
            break;
        }
        
        cv::Mat frame = cv::imdecode(buffer, cv::IMREAD_COLOR);
        if (!frame.empty()) {
            {
                std::lock_guard<std::mutex> lock(videoQueueMutex);
                // Clear old frames to prevent queue buildup
                while (videoFrameQueue.size() > 2) {
                    videoFrameQueue.pop();
                }
                videoFrameQueue.push(frame);
            }
            videoCond.notify_one();
        }
    }
    
    // Clean shutdown
    videoClientConnected = false;
    videoStreaming = false;
    shouldCloseWindow = true;
    videoSessionActive = false;
    videoCond.notify_one();
    mainThreadCond.notify_one();
    
    close(sockfd);
    logInfo("Video streaming ended from " + client_info);
}

void voiceUDPServer() {
    logInfo("Voice UDP server starting...");
    if (Pa_Initialize() != paNoError) {
        logError("Failed to init PortAudio.");
        return;
    }

    // Outer loop to allow multiple audio sessions
    while (running) {
        PaStream* stream = nullptr;
        int sockfd = -1;
        bool audioActive = false;
        sockaddr_in cliaddr{};
        socklen_t len = sizeof(cliaddr);
        char client_ip[INET_ADDRSTRLEN] = {0};
        int client_port = 0;

        try {
            if (Pa_OpenDefaultStream(&stream, 0, 1, paInt16, 44100, 512, nullptr, nullptr) != paNoError) {
                logError("Failed to open PortAudio output.");
                throw std::runtime_error("PortAudio open failed");
            }
            if (Pa_StartStream(stream) != paNoError) {
                logError("Failed to start PortAudio stream.");
                throw std::runtime_error("PortAudio start failed");
            }

            sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd < 0) {
                logError("Failed to create UDP voice socket.");
                throw std::runtime_error("Socket creation failed");
            }

            sockaddr_in servaddr{};
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(UDP_VOICE_PORT);
            servaddr.sin_addr.s_addr = INADDR_ANY;
            if (bind(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
                logError("Failed to bind UDP voice socket.");
                throw std::runtime_error("Socket bind failed");
            }

            char buffer[BUFFER_SIZE]; // Use BUFFER_SIZE for consistency
            
            logInfo("Voice UDP server listening for clients...");

            while (running) {
                ssize_t bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&cliaddr, &len);
                if (bytes <= 0) {
                    if (bytes == 0) {
                        logInfo("UDP client disconnected gracefully.");
                    } else {
                        logError("Error receiving UDP data: " + std::string(strerror(errno)));
                    }
                    break; // Break from inner loop to re-listen for new client
                }

                // Check for stop signal from client
                if (bytes == strlen("STOP_AUDIO") && std::string(buffer, bytes) == "STOP_AUDIO") {
                    if (audioActive) { // Only log if a session was active
                        logInfo("Audio streaming ended by client: " + std::string(client_ip) + ":" + std::to_string(client_port));
                    } else {
                        logInfo("Received STOP_AUDIO before session started, ignoring.");
                    }
                    break; // Exit inner loop, return to waiting for new client
                }

                // Log start when first audio packet arrives
                if (!audioActive) {
                    inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip, sizeof(client_ip));
                    client_port = ntohs(cliaddr.sin_port);
                    logInfo("Audio streaming started from " + std::string(client_ip) + ":" + std::to_string(client_port));
                    audioActive = true;
                }

                // Play audio
                Pa_WriteStream(stream, buffer, 512); // Assuming 512 samples per buffer
            }
        } catch (const std::exception& e) {
            logError("Voice UDP server error: " + std::string(e.what()));
        } catch (...) {
            logError("Unknown error in Voice UDP server.");
        }

        // Cleanup for the current session
        if (sockfd != -1) {
            close(sockfd);
            sockfd = -1;
        }
        if (stream) {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            stream = nullptr;
        }
        // Do NOT Pa_Terminate() here, as it's needed for subsequent sessions
    }
    Pa_Terminate(); // Terminate PortAudio only when the server is shutting down
    logInfo("Voice UDP server stopped.");
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
            // Wait for any existing video client to finish
            {
                std::lock_guard<std::mutex> lock(videoClientMutex);
                if (videoClientThread.joinable()) {
                    videoClientConnected = false;
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

void videoDisplayLoop() {
    bool windowCreated = false;
    while (running) {
        // Wait for video session to start
        {
            std::unique_lock<std::mutex> lock(mainThreadMutex);
            mainThreadCond.wait(lock, [] { return videoSessionActive || !running; });
        }
        
        if (!running) break;
        
        if (videoSessionActive) {
            logInfo("Video display loop started");
            windowCreated = false;
            
            while (videoSessionActive && running) {
                std::unique_lock<std::mutex> lock(videoQueueMutex);
                
                // Wait for frames or shutdown signal
                videoCond.wait_for(lock, std::chrono::milliseconds(30), 
                    [] { return !videoFrameQueue.empty() || shouldCloseWindow || !videoSessionActive; });
                
                if (shouldCloseWindow || !videoSessionActive) {
                    lock.unlock();
                    break;
                }
                
                if (!videoFrameQueue.empty()) {
                    cv::Mat frame = videoFrameQueue.front();
                    videoFrameQueue.pop();
                    lock.unlock();
                    
                    try {
                        cv::imshow("Live Video Feed", frame);
                        windowCreated = true;
                    } catch (const cv::Exception& e) {
                        logError("OpenCV error: " + std::string(e.what()));
                        break;
                    }
                } else {
                    lock.unlock();
                }
                
                // Process OpenCV events and check for ESC key
                int key = cv::waitKey(1) & 0xFF;
                if (key == 27) { // ESC key
                    logInfo("ESC pressed - closing video window");
                    videoStreaming = false;
                    videoClientConnected = false;
                    shouldCloseWindow = true;
                    videoSessionActive = false;
                    break;
                }
            }
            
            // Cleanup after video session ends
            if (windowCreated) {
                try {
                    cv::destroyWindow("Live Video Feed");
                    cv::waitKey(1); // Allow time for window destruction
                } catch (const cv::Exception& e) {
                    logError("Error destroying window: " + std::string(e.what()));
                }
            }
            
            // Clear any remaining frames
            {
                std::lock_guard<std::mutex> lock(videoQueueMutex);
                while (!videoFrameQueue.empty()) {
                    videoFrameQueue.pop();
                }
            }
            
            logInfo("Video display loop ended");
        }
    }
}

int main() {
    logInfo("Starting Mini Zoom Server...");
    
    std::thread udpThread(voiceUDPServer);
    std::thread tcpThread(tcpServer);
    
    // Run video display loop in main thread (required for macOS)
    videoDisplayLoop();
    
    logInfo("Shutting down server...");
    
    // Cleanup
    running = false;
    
    // Wake up any waiting threads
    mainThreadCond.notify_all();
    videoCond.notify_all();
    
    // Wait for video client thread to finish
    {
        std::lock_guard<std::mutex> lock(videoClientMutex);
        if (videoClientThread.joinable()) {
            videoClientConnected = false;
            videoClientThread.join();
        }
    }
    
    if (tcpThread.joinable()) tcpThread.join();
    if (udpThread.joinable()) udpThread.join();
    
    logInfo("Server shutdown complete.");
    return 0;
}