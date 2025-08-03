CXX = g++
CXXFLAGS = -Wall -O2 -std=c++17

# Dependencies
PKG_OPENCV = `pkg-config --cflags --libs opencv4`
PORTAUDIO_FLAGS = -lportaudio
UTILS = utils/common_utils.cpp

# Directories
CLIENT_DIR = client
SERVER_DIR = server

# Binaries
CLIENTS = \
	$(CLIENT_DIR)/chat_client \
	$(CLIENT_DIR)/file_client \
	$(CLIENT_DIR)/voice_client \
	$(CLIENT_DIR)/video_client

SERVERS = \
	$(SERVER_DIR)/chat_server \
	$(SERVER_DIR)/file_server \
	$(SERVER_DIR)/voice_server \
	$(SERVER_DIR)/video_server

all: $(CLIENTS) $(SERVERS)

$(CLIENT_DIR)/chat_client: $(CLIENT_DIR)/chat_client.cpp $(CLIENT_DIR)/utils/common_utils.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(CLIENT_DIR)/utils/common_utils.cpp

$(CLIENT_DIR)/file_client: $(CLIENT_DIR)/file_client.cpp $(CLIENT_DIR)/utils/common_utils.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(CLIENT_DIR)/utils/common_utils.cpp

$(CLIENT_DIR)/voice_client: $(CLIENT_DIR)/voice_client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(PORTAUDIO_FLAGS)

$(CLIENT_DIR)/video_client: $(CLIENT_DIR)/video_client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(PKG_OPENCV)

$(SERVER_DIR)/chat_server: $(SERVER_DIR)/chat_server.cpp $(SERVER_DIR)/utils/common_utils.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(SERVER_DIR)/utils/common_utils.cpp

$(SERVER_DIR)/file_server: $(SERVER_DIR)/file_server.cpp $(SERVER_DIR)/utils/common_utils.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(SERVER_DIR)/utils/common_utils.cpp

$(SERVER_DIR)/voice_server: $(SERVER_DIR)/voice_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(PORTAUDIO_FLAGS)

$(SERVER_DIR)/video_server: $(SERVER_DIR)/video_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(PKG_OPENCV)

clean:
	rm -f $(CLIENTS) $(SERVERS)

.PHONY: all clean