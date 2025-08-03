#ifndef VOICE_SERVER_H
#define VOICE_SERVER_H

#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <unistd.h>
#include <portaudio.h>
#include <cstring> // For strlen

#include "common_utils.h"
#include "server_common.h" // For running, UDP_VOICE_PORT, BUFFER_SIZE

inline void voiceUDPServer() {
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

#endif // VOICE_SERVER_H