#include <iostream>
#include <portaudio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "../utils/common_utils.h"

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512
#define PORT 7000

int main() {
    // Initialize PortAudio
    if (Pa_Initialize() != paNoError) {
        logError("Failed to initialize PortAudio.");
        return EXIT_FAILURE;
    }

    PaStream* stream;
    if (Pa_OpenDefaultStream(&stream, 0, 1, paInt16, SAMPLE_RATE, FRAMES_PER_BUFFER, nullptr, nullptr) != paNoError) {
        logError("Failed to open audio output stream.");
        Pa_Terminate();
        return EXIT_FAILURE;
    }

    if (Pa_StartStream(stream) != paNoError) {
        logError("Failed to start audio output stream.");
        Pa_CloseStream(stream);
        Pa_Terminate();
        return EXIT_FAILURE;
    }

    // Setup UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        logError("Failed to create UDP socket.");
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
        return EXIT_FAILURE;
    }

    sockaddr_in servaddr{}, cliaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    socklen_t len = sizeof(cliaddr);

    if (bind(sockfd, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) < 0) {
        logError("Failed to bind UDP socket.");
        close(sockfd);
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
        return EXIT_FAILURE;
    }

    char buffer[FRAMES_PER_BUFFER * 2];  // 2 bytes/sample for paInt16

    logInfo("Voice server listening on port " + std::to_string(PORT));
    logInfo("Press Ctrl+C to stop...");

    while (true) {
        ssize_t bytes = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                 reinterpret_cast<sockaddr*>(&cliaddr), &len);
        if (bytes > 0) {
            PaError err = Pa_WriteStream(stream, buffer, FRAMES_PER_BUFFER);
            if (err != paNoError) {
                logError("Failed to write to audio stream.");
                break;
            }
        } else {
            logError("Failed to receive UDP audio packet.");
        }
    }

    // Cleanup
    close(sockfd);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    logInfo("Voice server stopped.");
    return EXIT_SUCCESS;
}