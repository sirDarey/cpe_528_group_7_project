#ifndef SERVER_COMMON_H
#define SERVER_COMMON_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <thread> // For std::thread declaration

// Global constants
#define TCP_PORT 5000
#define UDP_VOICE_PORT 7000
#define BUFFER_SIZE 4096
#define MAX_CHAT_CLIENTS 50

// Mode IDs
#define MODE_CHAT  1
#define MODE_FILE  2
#define MODE_VIDEO 3

// Global flags (declared extern, defined in server_main.cpp)
extern volatile bool running;
extern std::atomic<bool> videoStreaming;
extern std::atomic<bool> videoClientConnected;
extern std::atomic<bool> shouldCloseWindow;
extern std::atomic<bool> videoSessionActive;

// Global chat client list and mutex
extern std::vector<int> chatClients;
extern std::mutex chatMutex;

// Thread-safe queue for video frames
extern std::queue<cv::Mat> videoFrameQueue;
extern std::mutex videoQueueMutex;
extern std::condition_variable videoCond;

// Video client thread tracking
extern std::thread videoClientThread;
extern std::mutex videoClientMutex;

// Main thread video control
extern std::condition_variable mainThreadCond;
extern std::mutex mainThreadMutex;

#endif // SERVER_COMMON_H