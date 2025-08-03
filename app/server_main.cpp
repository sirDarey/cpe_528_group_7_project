#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>

// Include common utilities
#include "common_utils.h" // For logInfo, logError (assuming it's a .cpp file or has inline functions)
#include "server_common.h"
#include "server_utils.h"

// Include handler/server headers (which now contain function implementations)
#include "chat_handler.h"
#include "file_handler.h"
#include "video_handler.h"
#include "voice_server.h"
#include "tcp_server.h"
#include "video_display.h"

// Define global variables declared in server_common.h
volatile bool running = true;
std::atomic<bool> videoStreaming{false};
std::atomic<bool> videoClientConnected{false};
std::atomic<bool> shouldCloseWindow{false};
std::atomic<bool> videoSessionActive{false};

std::vector<int> chatClients;
std::mutex chatMutex;

std::queue<cv::Mat> videoFrameQueue;
std::mutex videoQueueMutex;
std::condition_variable videoCond;

std::thread videoClientThread; // Defined here, managed by tcpServer
std::mutex videoClientMutex;

std::condition_variable mainThreadCond;
std::mutex mainThreadMutex;

int main() {
    logInfo("Starting Mini Zoom Server...");

    std::thread udpThread(voiceUDPServer); // From voice_server.h
    std::thread tcpThread(tcpServer);     // From tcp_server.h

    // Run video display loop in main thread (required for macOS GUI)
    videoDisplayLoop(); // From video_display.h

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
            videoClientConnected = false; // Ensure old thread exits
            videoClientThread.join();
        }
    }

    if (tcpThread.joinable()) tcpThread.join();
    if (udpThread.joinable()) udpThread.join();

    logInfo("Server shutdown complete.");
    return 0;
}