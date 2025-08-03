#include <iostream>
#include <thread>
#include <vector>
#include <fstream>
#include <arpa/inet.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include <portaudio.h>
#include <limits>
#include <iomanip> // For std::fixed and std::setprecision
#include <csignal> // For std::signal
#include <atomic>  // For std::atomic

#include "common_utils.h"

#define TCP_PORT 5000
#define UDP_VOICE_PORT 7000
#define BUFFER_SIZE 4096

#define MODE_CHAT  1
#define MODE_FILE  2
#define MODE_VIDEO 3

// Global flag to control the main application loop
volatile bool running = true;

// Global atomic flag for voice mode control
std::atomic<bool> voiceActive{false};

// Global signal handler for Ctrl+C (SIGINT)
void handleSigint(int signal) {
    if (signal == SIGINT) {
        logInfo("SIGINT received, stopping voice streaming...");
        voiceActive = false; // Signal the voice mode loop to stop
    }
}

// ---------- Chat ----------
void chatReceiver(int sockfd) {
    char buffer[1024];
    while (running) { // Use global 'running' for overall app control
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            logInfo("Chat server disconnected or error.");
            break;
        }
        buffer[bytes] = '\0';
        logInfo(std::string("[Chat] ") + buffer);
    }
}

void runChatMode(const char* server_ip) {
    logInfo("Attempting to connect for Chat...");
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        logError("Failed to create socket for Chat.");
        return;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(TCP_PORT);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        logError("Invalid server IP address for Chat.");
        close(sockfd);
        return;
    }

    if (connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        logError("Failed to connect for Chat: " + std::string(strerror(errno)));
        close(sockfd);
        return;
    }
    logInfo("Connected for Chat.");

    uint8_t mode = MODE_CHAT;
    if (!sendAll(sockfd, (char*)&mode, sizeof(mode))) {
        logError("Failed to send mode to server for Chat.");
        close(sockfd);
        return;
    }

    std::thread receiver(chatReceiver, sockfd);
    
    logInfo("Chat mode started. Type '/exit' to return to main menu.");
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "/exit") {
            logInfo("Exiting chat mode...");
            break;
        }
        if (!sendAll(sockfd, line.c_str(), line.size())) {
            logError("Failed to send message. Connection lost.");
            break;
        }
    }
    
    close(sockfd);
    if (receiver.joinable()) receiver.join();
    logInfo("Chat mode ended.");
}

// ---------- File ----------
void runFileMode(const char* server_ip) {
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

// ---------- Video ----------
void runVideoMode(const char* server_ip) {
    try {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            logError("Failed to create socket for video streaming");
            return;
        }
        
        sockaddr_in servaddr{};
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(TCP_PORT);
        
        if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
            logError("Invalid server IP address");
            close(sockfd);
            return;
        }
        
        if (connect(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
            logError("Failed to connect for Video: " + std::string(strerror(errno)));
            close(sockfd);
            return;
        }
        logInfo("Connected for Video streaming.");

        uint8_t mode = MODE_VIDEO;
        if (!sendAll(sockfd, (char*)&mode, sizeof(mode))) {
            logError("Failed to send mode to server.");
            close(sockfd);
            return;
        }
        
        cv::VideoCapture cap;
        logInfo("Attempting to open camera...");
        try {
            cap.open(0, cv::CAP_AVFOUNDATION); // Use CAP_AVFOUNDATION for macOS
            if (!cap.isOpened()) {
                logError("Could not open camera. Make sure your camera is not being used by another application.");
                close(sockfd);
                return;
            }
        } catch (const cv::Exception& e) {
            logError("OpenCV camera open error: " + std::string(e.what()));
            close(sockfd);
            return;
        }
        logInfo("Camera opened successfully.");
        
        // Set camera properties for better performance
        try {
            cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
            cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
            cap.set(cv::CAP_PROP_FPS, 30);
            logInfo("Camera properties set to 640x480 @ 30 FPS.");
        } catch (...) {
            logInfo("Could not set camera properties, using defaults.");
        }
        
        logInfo("Video streaming started. Press ESC to stop and return to main menu.");
        
        cv::Mat frame;
        std::vector<uchar> encoded;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 40};
        
        bool streaming = true;
        // Removed local_preview_window_created flag as we are removing local preview
        int frameCount = 0;
        auto startTime = std::chrono::steady_clock::now();
        
        while (streaming && running) {
            try {
                bool frameRead = cap.read(frame);
                if (!frameRead || frame.empty()) {
                    logError("Failed to capture frame from camera or stream ended.");
                    break;
                }
                
                // Removed local preview display to avoid macOS GUI conflicts
                // cv::imshow("Local Preview", frame);
                
                // Encode frame
                encoded.clear();
                if (!cv::imencode(".jpg", frame, encoded, params)) {
                    logError("Failed to encode frame.");
                    break;
                }
                
                // Send frame size
                uint32_t frame_size_net = htonl((uint32_t)encoded.size());
                if (!sendAll(sockfd, (char*)&frame_size_net, sizeof(frame_size_net))) {
                    logInfo("Server disconnected or connection lost during frame size send.");
                    break;
                }
                
                // Send frame data
                if (!sendAll(sockfd, (char*)encoded.data(), encoded.size())) {
                    logInfo("Server disconnected or connection lost during frame data send.");
                    break;
                }
                
                frameCount++;
                
                // Show frame rate every 30 frames
                if (frameCount % 30 == 0) {
                    auto currentTime = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
                    if (duration.count() > 0) {
                        double fps = (30.0 * 1000.0) / duration.count();
                        std::cout << "\rStreaming... FPS: " << std::fixed << std::setprecision(1) << fps 
                                 << " | Press ESC to stop" << std::flush;
                    }
                    startTime = currentTime;
                }
                
                // Check for ESC key (non-blocking)
                // cv::waitKey(1) is still needed to process any pending GUI events, even without imshow
                int key = cv::waitKey(1) & 0xFF; 
                if (key == 27) { // ESC
                    logInfo("\nESC pressed - stopping video stream.");
                    streaming = false;
                }
                
                // Small delay to prevent overwhelming the server
                std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
                
            } catch (const cv::Exception& e) {
                logError("OpenCV error during streaming loop: " + std::string(e.what()));
                break;
            } catch (const std::exception& e) {
                logError("Standard exception during streaming loop: " + std::string(e.what()));
                break;
            } catch (...) {
                logError("Unknown exception during streaming loop.");
                break;
            }
        }
        
        // Send end signal to server
        try {
            uint32_t end_signal = htonl(0);
            sendAll(sockfd, (char*)&end_signal, sizeof(end_signal));
            logInfo("End signal sent to server.");
        } catch (...) {
            logError("Error sending end signal to server (ignored).");
        }
        
        // Cleanup
        try {
            cap.release();
            logInfo("Camera released.");
        } catch (...) {
            logError("Error releasing camera (ignored).");
        }
        
        close(sockfd);
        logInfo("Socket closed.");
        
        // Ensure all OpenCV windows are destroyed, even if not explicitly created by this mode
        try {
            cv::destroyAllWindows();
            cv::waitKey(1); // Give time for windows to be destroyed
            logInfo("All OpenCV windows destroyed.");
        } catch (const cv::Exception& e) {
            logError("Error destroying OpenCV windows: " + std::string(e.what()));
        } catch (...) {
            logError("Unknown error destroying OpenCV windows.");
        }
        
        std::cout << std::endl; // Ensure new line after FPS display
        logInfo("Video streaming stopped. Returning to main menu.");
        
    } catch (const std::exception& e) {
        logError("Exception in runVideoMode: " + std::string(e.what()));
    } catch (...) {
        logError("Unknown exception in runVideoMode.");
    }
    
    // Ensure we always return to menu
    logInfo("Video mode function completed, returning to main menu.");
}

// ---------- Voice ----------
void runVoiceMode(const char* server_ip) {
    // Store the current SIGINT handler to restore it later
    void (*old_sigint_handler)(int) = std::signal(SIGINT, handleSigint);

    voiceActive = true; // Set the global atomic flag to true for this session

    if (Pa_Initialize() != paNoError) {
        logError("Failed to initialize PortAudio.");
        std::signal(SIGINT, old_sigint_handler); // Restore handler on error
        return;
    }
    
    PaStream* stream;
    if (Pa_OpenDefaultStream(&stream, 1, 0, paInt16, 44100, 512, nullptr, nullptr) != paNoError) {
        logError("Failed to open audio input stream.");
        Pa_Terminate();
        std::signal(SIGINT, old_sigint_handler); // Restore handler on error
        return;
    }
    
    if (Pa_StartStream(stream) != paNoError) {
        logError("Failed to start audio stream.");
        Pa_CloseStream(stream);
        Pa_Terminate();
        std::signal(SIGINT, old_sigint_handler); // Restore handler on error
        return;
    }
    
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        logError("Failed to create UDP socket.");
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
        std::signal(SIGINT, old_sigint_handler); // Restore handler on error
        return;
    }
    
    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(UDP_VOICE_PORT);
    inet_pton(AF_INET, server_ip, &servaddr.sin_addr);
    
    char buffer[512 * 2];
    logInfo("Voice streaming started. Press Ctrl+C to stop and return to main menu.");
    
    try {
        while (voiceActive) { // Loop controlled by the global atomic flag
            PaError err = Pa_ReadStream(stream, buffer, 512);
            if (err != paNoError) {
                logError("Error reading from audio stream.");
                break;
            }
            
            ssize_t sent = sendto(sockfd, buffer, sizeof(buffer), 0, 
                                (sockaddr*)&servaddr, sizeof(servaddr));
            if (sent < 0) {
                logError("Failed to send audio data.");
                break;
            }
        }
    } catch (...) {
        logInfo("Voice streaming interrupted.");
    }
    
    // Send a stop signal to the server
    const char* stop_msg = "STOP_AUDIO";
    sendto(sockfd, stop_msg, strlen(stop_msg), 0, (sockaddr*)&servaddr, sizeof(servaddr));
    logInfo("Sent STOP_AUDIO signal to server.");

    close(sockfd);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    
    // Restore the original SIGINT handler before exiting the function
    std::signal(SIGINT, old_sigint_handler);
    logInfo("Voice streaming stopped. Returning to main menu.");
}

// ---------- Utility Functions ----------
void clearInputBuffer() {
    try {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        logInfo("Input buffer cleared.");
    } catch (...) {
        logError("Error clearing input buffer.");
    }
}

void showMenu() {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "           MINI ZOOM CLIENT" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "1. Chat Mode (type '/exit' to return)" << std::endl;
    std::cout << "2. File Transfer" << std::endl;
    std::cout << "3. Video Streaming (ESC to return)" << std::endl;
    std::cout << "4. Voice Streaming (Ctrl+C to return)" << std::endl;
    std::cout << "5. Exit Application" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "Choice: ";
}

int getMenuChoice() {
    std::string input;
    std::getline(std::cin, input);
    
    if (input.empty()) {
        return -1;
    }
    
    // Check if input contains only digits
    for (char c : input) {
        if (!std::isdigit(c)) {
            return -1;
        }
    }
    
    try {
        int choice = std::stoi(input);
        return (choice >= 1 && choice <= 5) ? choice : -1;
    } catch (...) {
        return -1;
    }
}

// ---------- Main Loop ----------
int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cout << "Usage: " << argv[0] << " <server_ip>" << std::endl;
            std::cout << "Example: " << argv[0] << " 127.0.0.1" << std::endl;
            return EXIT_FAILURE;
        }
        
        const char* server_ip = argv[1];
        
        logInfo("Mini Zoom Client started. Server: " + std::string(server_ip));
        
        while (running) {
            try {
                showMenu();
                
                int choice = getMenuChoice();
                
                if (choice == -1) {
                    logError("Invalid choice. Please enter a number between 1 and 5.");
                    continue;
                }
                
                std::cout << std::endl;
                logInfo("Selected choice: " + std::to_string(choice));
                
                switch (choice) {
                    case 1:
                        logInfo("Starting chat mode...");
                        runChatMode(server_ip);
                        logInfo("Chat mode completed, back to main menu.");
                        break;
                        
                    case 2:
                        logInfo("Starting file transfer mode...");
                        runFileMode(server_ip);
                        logInfo("File transfer completed, back to main menu.");
                        break;
                        
                    case 3:
                        logInfo("Starting video streaming mode...");
                        runVideoMode(server_ip);
                        logInfo("Video streaming completed, back to main menu.");
                        // Clear any remaining input after video mode
                        // clearInputBuffer();
                        break;
                        
                    case 4:
                        logInfo("Starting voice streaming mode...");
                        runVoiceMode(server_ip);
                        logInfo("Voice streaming completed, back to main menu.");
                        // clearInputBuffer();
                        break;
                        
                    case 5:
                        logInfo("Exiting Mini Zoom Client...");
                        running = false;
                        break;
                        
                    default:
                        logError("Invalid choice. Please enter a number between 1 and 5.");
                        break;
                }
                
                // Small delay before showing menu again
                if (running) {
                    logInfo("Preparing to show menu again...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
                
            } catch (const std::exception& e) {
                logError("Exception in main loop: " + std::string(e.what()));
                logInfo("Continuing to main menu...");
                clearInputBuffer();
            } catch (...) {
                logError("Unknown exception in main loop");
                logInfo("Continuing to main menu...");
                clearInputBuffer();
            }
        }
        
        logInfo("Mini Zoom Client terminated normally.");
        return 0;
        
    } catch (const std::exception& e) {
        logError("Fatal exception: " + std::string(e.what()));
        return EXIT_FAILURE;
    } catch (...) {
        logError("Fatal unknown exception");
        return EXIT_FAILURE;
    }
}