#ifndef CLIENT_COMMON_H
#define CLIENT_COMMON_H

#include <atomic>
#include <csignal> // For std::signal

// Global constants
#define TCP_PORT 5000
#define UDP_VOICE_PORT 7000
#define BUFFER_SIZE 4096

#define MODE_CHAT  1
#define MODE_FILE  2
#define MODE_VIDEO 3

// Global flags (declared extern, defined in client_main.cpp)
extern volatile bool running;
extern std::atomic<bool> voiceActive;

// Global signal handler declaration
extern void handleSigint(int signal);

#endif // CLIENT_COMMON_H