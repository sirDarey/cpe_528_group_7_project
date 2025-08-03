#ifndef VIDEO_HANDLER_H
#define VIDEO_HANDLER_H

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unistd.h> // For close
#include <opencv2/opencv.hpp>

#include "server_utils.h"
#include "common_utils.h"
#include "server_common.h" // For running, videoClientConnected, videoStreaming, videoSessionActive, shouldCloseWindow, videoQueueMutex, videoFrameQueue, videoCond, mainThreadCond

inline void handleVideoClient(int sockfd) {
    std::string client_info = getClientInfo(sockfd); // getClientInfo from server_utils.h
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
    mainThreadCond.notify_one(); // Ensure main thread is woken up for cleanup
    
    close(sockfd);
    logInfo("Video streaming ended from " + client_info);
}

#endif // VIDEO_HANDLER_H