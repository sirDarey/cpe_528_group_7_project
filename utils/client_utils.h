#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#include <iostream>
#include <string>
#include <limits>
#include <termios.h> // For termios
#include <fcntl.h>   // For fcntl
#include <unistd.h>  // For STDIN_FILENO

#include "common_utils.h" // For logInfo, logError

// Non-blocking character read utility
// This function handles setting and restoring non-blocking mode locally.
inline int getch_nonblocking() {
    int ch = -1;
    // Set stdin to non-blocking mode temporarily
    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    ch = getchar(); // Attempt to read a character
    
    // Restore stdin to its original blocking mode
    fcntl(STDIN_FILENO, F_SETFL, oldf); 
    return ch;
}

// Utility function to clear input buffer
inline void clearInputBuffer() {
    try {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        logInfo("Input buffer cleared.");
    } catch (...) {
        logError("Error clearing input buffer.");
    }
}

// Function to display the main menu
inline void showMenu() {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "           MINI ZOOM CLIENT" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "1. Chat Mode (type '/exit' to return)" << std::endl;
    std::cout << "2. File Transfer" << std::endl;
    std::cout << "3. Video Streaming (ESC to return)" << std::endl;
    std::cout << "4. Voice Streaming (Ctrl+C to return)" << std::endl;
    std::cout << "5. Exit Application" << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "Choice: ";
}

// Function to get menu choice from user
inline int getMenuChoice() {
    std::string input;
    std::getline(std::cin, input); 
    
    if (input.empty()) {
        return -1;
    }
    
    // Check if input contains only digits
    for (char c : input) {
        if (!std::isdigit(c)) {
            return -1;
        }
    }
    
    try {
        int choice = std::stoi(input);
        return (choice >= 1 && choice <= 5) ? choice : -1;
    } catch (...) {
        return -1;
    }
}

#endif // CLIENT_UTILS_H