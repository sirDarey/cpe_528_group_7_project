#ifndef VOICE_MODE_H
#define VOICE_MODE_H

#include <iostream>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>
#include <portaudio.h>
#include <csignal> // For std::signal
#include <atomic>  // For std::atomic
#include <cstring> // For strlen

#include "common_utils.h"
#include "client_common.h" // For UDP_VOICE_PORT, voiceActive, handleSigint

// Main function for voice streaming mode
inline void runVoiceMode(const char* server_ip) {
    // Store the current SIGINT handler to restore it later
    void (*old_sigint_handler)(int) = std::signal(SIGINT, handleSigint);

    voiceActive = true; // Set the global atomic flag to true for this session
    logInfo("Starting voice streaming mode...");

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

#endif // VOICE_MODE_H