#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <string>
#include <cstdint>
#include <vector>
#include <iostream>
#include <arpa/inet.h>

#ifdef __APPLE__
  #include <libkern/OSByteOrder.h>
  #ifndef htonll
    #define htonll(x) OSSwapHostToBigInt64(x)
  #endif
  #ifndef ntohll
    #define ntohll(x) OSSwapBigToHostInt64(x)
  #endif
#else
  #include <arpa/inet.h>
  static inline uint64_t htonll(uint64_t x) {
    return ((uint64_t)htonl(x & 0xFFFFFFFF) << 32) | htonl(x >> 32);
  }

  static inline uint64_t ntohll(uint64_t x) {
    return ((uint64_t)ntohl(x & 0xFFFFFFFF) << 32) | ntohl(x >> 32);
  }
#endif

// Send all data (TCP)
bool sendAll(int sockfd, const char* data, size_t len);

// Receive all data (TCP)
bool recvAll(int sockfd, char* buffer, size_t len);

// Logging
void logInfo(const std::string& msg);
void logError(const std::string& msg);

#endif // COMMON_UTILS_H