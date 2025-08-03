#include <iostream>
#include <string>
#include <thread>
#include <csignal> // For std::signal

// Include common definitions and utilities first
#include "common_utils.h" // For logInfo, logError (assuming it's a .cpp file or has inline functions)
#include "client_common.h"
#include "client_utils.h"

// Include feature headers (which now contain function implementations)
#include "chat_mode.h"
#include "file_mode.h"
#include "video_mode.h"
#include "voice_mode.h"

// Define global variables declared in client_common.h
volatile bool running = true;
std::atomic<bool> voiceActive{false};

// Define the global signal handler declared in client_common.h
void handleSigint(int signal) {
    if (signal == SIGINT) {
        logInfo("SIGINT received, stopping voice streaming...");
        voiceActive = false; // Signal the voice mode loop to stop
    }
}

// ---------- Main Loop ----------
int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cout << "Usage: " << argv[0] << " <server_ip>" << std::endl;
            std::cout << "Example: " << argv[0] << " 127.0.0.1" << std::endl;
            return EXIT_FAILURE;
        }
        
        const char* server_ip = argv[1];
        
        logInfo("Mini Zoom Client started. Server: " + std::string(server_ip));
        
        while (running) {
            try {
                showMenu(); // From client_utils.h
                
                int choice = getMenuChoice(); // From client_utils.h
                
                if (choice == -1) {
                    logError("Invalid choice. Please enter a number between 1 and 5.");
                    continue;
                }
                
                std::cout << std::endl;
                logInfo("Selected choice: " + std::to_string(choice));
                
                switch (choice) {
                    case 1:
                        logInfo("Starting chat mode...");
                        runChatMode(server_ip); // From chat_mode.h
                        logInfo("Chat mode completed, back to main menu.");
                        break;
                        
                    case 2:
                        logInfo("Starting file transfer mode...");
                        runFileMode(server_ip); // From file_mode.h
                        logInfo("File transfer completed, back to main menu.");
                        break;
                        
                    case 3:
                        logInfo("Starting video streaming mode...");
                        runVideoMode(server_ip); // From video_mode.h
                        logInfo("Video streaming completed, back to main menu.");
                        break;
                        
                    case 4:
                        logInfo("Starting voice streaming mode...");
                        runVoiceMode(server_ip); // From voice_mode.h
                        logInfo("Voice streaming completed, back to main menu.");
                        break;
                        
                    case 5:
                        logInfo("Exiting Mini Zoom Client...");
                        running = false;
                        break;
                        
                    default:
                        logError("Invalid choice. Please enter a number between 1 and 5.");
                        break;
                }
                
                // Small delay before showing menu again
                if (running) {
                    logInfo("Preparing to show menu again...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
                
            } catch (const std::exception& e) {
                logError("Exception in main loop: " + std::string(e.what()));
                logInfo("Continuing to main menu...");
                clearInputBuffer(); // From client_utils.h
            } catch (...) {
                logError("Unknown exception in main loop");
                logInfo("Continuing to main menu...");
                clearInputBuffer(); // From client_utils.h
            }
        }
        
        logInfo("Mini Zoom Client terminated normally.");
        return 0;
        
    } catch (const std::exception& e) {
        logError("Fatal exception: " + std::string(e.what()));
        return EXIT_FAILURE;
    } catch (...) {
        logError("Fatal unknown exception");
        return EXIT_FAILURE;
    }
}