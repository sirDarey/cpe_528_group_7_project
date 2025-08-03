#include <iostream>
#include <thread>
#include <vector>
#include <fstream>
#include <arpa/inet.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include <portaudio.h>
#include "common_utils.h"

#define TCP_PORT 5000
#define UDP_VOICE_PORT 7000
#define BUFFER_SIZE 4096

#define MODE_CHAT  1
#define MODE_FILE  2
#define MODE_VIDEO 3

volatile bool running = true;

// ---------- Chat ----------
void chatReceiver(int sockfd) {
    char buffer[1024];
    while (running) {
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        logInfo(std::string("[Chat] ") + buffer);
    }
}

void runChatMode(const char* server_ip) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, server_ip, &servaddr.sin_addr);

    if (connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        logError("Failed to connect for Chat: " + std::string(strerror(errno)));
        close(sockfd);
        return;
    }

    uint8_t mode = MODE_CHAT;
    sendAll(sockfd, (char*)&mode, sizeof(mode));

    std::thread receiver(chatReceiver, sockfd);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "/exit") break;
        if (!sendAll(sockfd, line.c_str(), line.size())) break;
    }
    close(sockfd);
    if (receiver.joinable()) receiver.join();
}

// ---------- File ----------
void runFileMode(const char* server_ip) {
    std::string file_path;
    std::cout << "Enter file path to send: ";
    std::getline(std::cin, file_path);

    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        logError("Error opening file.");
        return;
    }

    std::string filename = file_path.substr(file_path.find_last_of("/\\") + 1);
    file.seekg(0, std::ios::end);
    uint64_t file_size = file.tellg();
    file.seekg(0);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, server_ip, &servaddr.sin_addr);

    if (connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        logError("Failed to connect for File transfer: " + std::string(strerror(errno)));
        close(sockfd);
        return;
    }

    uint8_t mode = MODE_FILE;
    sendAll(sockfd, (char*)&mode, sizeof(mode));

    uint32_t name_len_net = htonl((uint32_t)filename.size());
    sendAll(sockfd, (char*)&name_len_net, sizeof(name_len_net));
    sendAll(sockfd, filename.c_str(), filename.size());
    uint64_t size_net = htonll(file_size);
    sendAll(sockfd, (char*)&size_net, sizeof(size_net));

    char buffer[BUFFER_SIZE];
    while (file) {
        file.read(buffer, BUFFER_SIZE);
        std::streamsize bytes = file.gcount();
        if (bytes > 0 && !sendAll(sockfd, buffer, bytes)) break;
    }
    file.close();
    close(sockfd);
    logInfo("File sent successfully.");
}

// ---------- Video ----------
void runVideoMode(const char* server_ip) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, server_ip, &servaddr.sin_addr);

    if (connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        logError("Failed to connect for Video: " + std::string(strerror(errno)));
        close(sockfd);
        return;
    }

    uint8_t mode = MODE_VIDEO;
    sendAll(sockfd, (char*)&mode, sizeof(mode));

    cv::VideoCapture cap(0, cv::CAP_AVFOUNDATION);
    if (!cap.isOpened()) {
        logError("Could not open camera.");
        close(sockfd);
        return;
    }

    cv::Mat frame;
    std::vector<uchar> encoded;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 40};

    while (true) {
        cap >> frame;
        if (frame.empty()) break;
        cv::imencode(".jpg", frame, encoded, params);
        uint32_t frame_size_net = htonl((uint32_t)encoded.size());
        if (!sendAll(sockfd, (char*)&frame_size_net, sizeof(frame_size_net))) break;
        if (!sendAll(sockfd, (char*)encoded.data(), encoded.size())) break;
        if (cv::waitKey(10) == 27) break; // ESC to exit
    }

    uint32_t end_signal = 0;
    end_signal = htonl(end_signal);
    sendAll(sockfd, (char*)&end_signal, sizeof(end_signal));

    cap.release();
    close(sockfd);
}

// ---------- Voice ----------
void runVoiceMode(const char* server_ip) {
    if (Pa_Initialize() != paNoError) {
        logError("Failed to init PortAudio.");
        return;
    }
    PaStream* stream;
    if (Pa_OpenDefaultStream(&stream, 1, 0, paInt16, 44100, 512, nullptr, nullptr) != paNoError) {
        logError("Failed to open audio stream.");
        Pa_Terminate();
        return;
    }
    Pa_StartStream(stream);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(UDP_VOICE_PORT);
    inet_pton(AF_INET, server_ip, &servaddr.sin_addr);

    char buffer[512 * 2];
    logInfo("Recording... Press Ctrl+C to stop.");
    while (true) {
        Pa_ReadStream(stream, buffer, 512);
        sendto(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&servaddr, sizeof(servaddr));
    }
    close(sockfd);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}

// ---------- Main Loop ----------
int main(int argc, char* argv[]) {
    if (argc != 2) {
        logError("Usage: ./unified_client <server_ip>");
        return EXIT_FAILURE;
    }
    const char* server_ip = argv[1];

    while (true) {
        std::cout << "\nSelect mode:\n";
        std::cout << "1. Chat (/exit to return)\n";
        std::cout << "2. File Transfer\n";
        std::cout << "3. Video Streaming (ESC to return)\n";
        std::cout << "4. Voice Streaming (Ctrl+C to return)\n";
        std::cout << "5. Exit\n";
        std::cout << "Choice: ";

        std::string input;
        std::getline(std::cin, input);

        if (input.empty() || !std::all_of(input.begin(), input.end(), ::isdigit)) {
            logError("Invalid choice. Please enter a number between 1 and 5.");
            continue;
        }

        int choice = std::stoi(input);

        if (choice == 1) runChatMode(server_ip);
        else if (choice == 2) runFileMode(server_ip);
        else if (choice == 3) {
            runVideoMode(server_ip);
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        else if (choice == 4) runVoiceMode(server_ip);
        else if (choice == 5) break;
        else logError("Invalid choice. Please enter a number between 1 and 5.");
    }

    logInfo("Client exited.");
    return 0;
}