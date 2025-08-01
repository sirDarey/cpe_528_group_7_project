#include <iostream>
#include <portaudio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "common_utils.h"

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512
#define PORT 7000

int main(int argc, char* argv[]) {
    if (argc != 2) {
        logError("Usage: ./voice_client <server_ip>");
        return EXIT_FAILURE;
    }

    const char* server_ip = argv[1];

    // Initialize PortAudio
    if (Pa_Initialize() != paNoError) {
        logError("Failed to initialize PortAudio.");
        return EXIT_FAILURE;
    }

    PaStream* stream;
    if (Pa_OpenDefaultStream(&stream, 1, 0, paInt16, SAMPLE_RATE, FRAMES_PER_BUFFER, nullptr, nullptr) != paNoError) {
        logError("Failed to open PortAudio stream.");
        Pa_Terminate();
        return EXIT_FAILURE;
    }

    if (Pa_StartStream(stream) != paNoError) {
        logError("Failed to start PortAudio stream.");
        Pa_CloseStream(stream);
        Pa_Terminate();
        return EXIT_FAILURE;
    }

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        logError("Failed to create UDP socket.");
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
        return EXIT_FAILURE;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        logError("Invalid server IP address.");
        close(sockfd);
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
        return EXIT_FAILURE;
    }

    char buffer[FRAMES_PER_BUFFER * 2]; // 2 bytes per sample (paInt16)

    logInfo("Sending microphone audio to " + std::string(server_ip) + ":" + std::to_string(PORT));
    logInfo("Press Ctrl+C to stop...");

    while (true) {
        PaError err = Pa_ReadStream(stream, buffer, FRAMES_PER_BUFFER);
        if (err != paNoError) {
            logError("Failed to read from audio stream.");
            break;
        }

        ssize_t sent = sendto(sockfd, buffer, sizeof(buffer), 0,
                              reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr));
        if (sent < 0) {
            logError("Failed to send UDP audio packet.");
            break;
        }
    }

    // Cleanup
    close(sockfd);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    logInfo("Audio streaming stopped.");
    return EXIT_SUCCESS;
}