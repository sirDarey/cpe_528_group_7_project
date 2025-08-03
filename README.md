# Mini-Zoom: Real-Time Communication App

## 🧱 Features
✅ Real-time text chat  
✅ Binary-safe file transfer (image/audio/video)  
✅ Live voice streaming (microphone/speaker)  
✅ Webcam-based video streaming (camera-to-camera)

## 🛠 Requirements
- Linux (Ubuntu preferred)
- g++
- OpenCV: `sudo apt install libopencv-dev`
- PortAudio: `sudo apt install portaudio19-dev`

## ⚙️ Setup

```bash
git clone <this-repo>
cd mini-zoom
sudo apt update
sudo apt install libopencv-dev
sudo apt install libopencv-dev portaudio19-dev
make
make clean
```

## 🚀 Running

📤 Chat

```bash
# On Server
./server/chat_server

# On Client
./client/chat_client <server_ip>
```

📁 File Transfer
```bash
# On Server
./server/file_server

# On Client
./client/file_client <server_ip> <path/to/file>
```

🎙 Voice
```bash
# On Server
./server/voice_server

# On Client
./client/voice_client <server_ip>
```

🎥 Video
```bash
# On Server
./server/video_server

# On Client
./client/video_client <server_ip>
```

Press ESC to quit video/audio streaming.


## 🧠 Architecture
```
mini-zoom/
├── client/
│   ├── chat_client.cpp
│   ├── file_client.cpp
│   ├── voice_client.cpp
│   └── video_client.cpp
├── server/
│   ├── chat_server.cpp
│   ├── file_server.cpp
│   ├── voice_server.cpp
│   └── video_server.cpp
├── utils/
│   ├── common_utils.cpp
│   └── common_utils.h
├── README.md
├── Makefile
└── demo.mp4
```


## 💡 Highlights
	•	Socket Programming: TCP for chat & file transfer, UDP for voice/video
	•	Multimedia Streaming: PortAudio & OpenCV
	•	Concurrency: Multi-client text chat using select()
	•	Binary-safe Protocols: Handles arbitrary files
	•	Code Modularity: Reusable utilities in common_utils.cpp

## 🎬 Demo

📽 Coming soon: demo.mp4 walkthrough of all features!


<!-- g++ voice_client.cpp -o voice_client -lportaudio -lpthread  
g++ voice_server.cpp -o voice_server -lportaudio -lpthread


I had to install opencv and portaudio at some point
brew install opencv
brew install portaudio


-------  COMPILATION ------

g++ chat_server.cpp ../utils/common_utils.cpp -o chat_server -std=c++17

g++ video_client.cpp ../utils/common_utils.cpp -o video_client \
    -std=c++17 -I../utils \
    -I/opt/homebrew/include/opencv4 \
    -L/opt/homebrew/lib \
    -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lopencv_videoio -lopencv_imgproc


g++ voice_server.cpp ../utils/common_utils.cpp -o voice_server \
    -std=c++17 -I../utils \
    -I/opt/homebrew/include -L/opt/homebrew/lib -lportaudio


g++ unified_server.cpp utils/common_utils.cpp -o unified_server \
    -std=c++17 -I utils \
    -I/opt/homebrew/include \
    -I/opt/homebrew/include/opencv4 \
    -L/opt/homebrew/lib \
    -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lopencv_videoio -lopencv_imgproc \
    -lportaudio

g++ unified_client.cpp utils/common_utils.cpp -o unified_client \
    -std=c++17 -I utils \
    -I/opt/homebrew/include \
    -I/opt/homebrew/include/opencv4 \
    -L/opt/homebrew/lib \
    -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lopencv_videoio -lopencv_imgproc \
    -lportaudio

better logging
max clients conn
for every new conn, show number of slots left
-->