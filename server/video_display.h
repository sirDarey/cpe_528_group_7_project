#ifndef VIDEO_DISPLAY_H
#define VIDEO_DISPLAY_H

#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono> // For std::chrono
#include <opencv2/opencv.hpp>

#include "common_utils.h"
#include "server_common.h"

inline void videoDisplayLoop() {
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

#endif // VIDEO_DISPLAY_H