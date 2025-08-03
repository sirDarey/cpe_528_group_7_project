#ifndef VIDEO_MODE_H
#define VIDEO_MODE_H

#include <iostream>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include <limits>
#include <iomanip> // For std::fixed and std::setprecision
#include <chrono>  // For std::chrono

#include "common_utils.h"
#include "client_common.h" // For TCP_PORT, MODE_VIDEO, running
#include "client_utils.h" // For getch_nonblocking

// Main function for video streaming mode
inline void runVideoMode(const char* server_ip) {
    try {
        logInfo("Starting video streaming mode...");
        
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
            cap.open(0); // Use cap.open(0) for general compatibility
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
        int frameCount = 0;
        auto startTime = std::chrono::steady_clock::now();
        
        while (streaming && running) {
            try {
                bool frameRead = cap.read(frame);
                if (!frameRead || frame.empty()) {
                    logError("Failed to capture frame from camera or stream ended.");
                    break;
                }
                
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
                
                // Check for ESC key using non-blocking input
                int key = getch_nonblocking();
                if (key == 27) { // ESC key ASCII value
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

#endif // VIDEO_MODE_H