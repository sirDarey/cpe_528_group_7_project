# Mini-Zoom: Real-Time Communication App

Mini-Zoom is a command-line application for real-time communication with features for text chat, file transfers, and live audio and video streaming. It's built with C++ and uses various libraries for networking and multimedia.

## 🚀 Features

* **Real-time text chat**
* **Binary-safe file transfer** (supports any file type)
* **Live voice streaming** (microphone to speaker)
* **Webcam-based video streaming** (camera to camera)

---

## 🛠 Requirements

To compile and run Mini-Zoom, you'll need the following:

* **C++ Compiler:** A `g++` compiler that supports C++17 or newer.
* **OpenCV:** A library for video capture and display.
* **PortAudio:** A library for audio input and output.
* **Operating System:** macOS or Linux (Ubuntu/Debian distributions are preferred).

### Installation of Dependencies

#### For Linux (Ubuntu/Debian):

```bash
# Update the package list
sudo apt update

# Install g++
sudo apt install build-essential

# Install OpenCV development libraries
sudo apt install libopencv-dev

# Install PortAudio development libraries
sudo apt install portaudio19-dev
````

#### For macOS (using Homebrew):

```shellscript
# Install Homebrew if you don't have it
/bin/bash -c "$(curl -fsSL [https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh](https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh))"

# Install g++ (comes with Xcode Command Line Tools, or via Homebrew)
brew install gcc

# Install OpenCV
brew install opencv

# Install PortAudio
brew install portaudio
```

-----

## ️ Setup and Compilation

1.  **Clone the repository:**

```shellscript
git clone git@github.com:sirDarey/cpe_528_group_7_project.git
cd mini-zoom
````

2.  **Compile the applications:**

Navigate to the root directory of the `mini-zoom` project.

**For macOS (using Homebrew):**

1.  **Compile the Client:**

<!-- end list -->

```shellscript
g++ app/client_main.cpp utils/common_utils.cpp -o client_app \
    -std=c++17 -Iclient -I utils \
    -I/opt/homebrew/include \
    -I/opt/homebrew/include/opencv4 \
    -L/opt/homebrew/lib \
    -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lopencv_videoio -lopencv_imgproc \
    -lportaudio
```

2.  **Compile the Server:**

<!-- end list -->

```shellscript
g++ app/server_main.cpp utils/common_utils.cpp -o server_app \
    -std=c++17 -Iserver -I utils \
    -I/opt/homebrew/include \
    -I/opt/homebrew/include/opencv4 \
    -L/opt/homebrew/lib \
    -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lopencv_videoio -lopencv_imgproc \
    -lportaudio
```

**For Linux:**

1.  **Compile the Client:**

<!-- end list -->

```shellscript
g++ app/client_main.cpp utils/common_utils.cpp \
    -o client_app \
    -std=c++17 \
    -Iclient -Iutils \
    $(pkg-config --cflags opencv4) \
    -L/usr/local/lib \
    $(pkg-config --libs opencv4) \
    -lportaudio \
    -lpthread
```

2.  **Compile the Server:**

<!-- end list -->

```shellscript
g++ app/server_main.cpp utils/common_utils.cpp \
    -o server_app \
    -std=c++17 \
    -Iserver -Iutils \
    $(pkg-config --cflags opencv4) \
    -L/usr/local/lib \
    $(pkg-config --libs opencv4) \
    -lportaudio \
    -lpthread
```

> **Note:** The `-I/usr/local/include` and `-L/usr/local/lib` flags are common paths for standard installations on Linux. You might need to adjust these if your libraries are installed in different locations.

-----

## Running the Application

First, start the server, then connect the clients to it.

1.  **Start the Server:**

Open a terminal and run the following command. The server will start listening for connections.

```shellscript
./server_app
```

2.  **Start the Client:**

Open a separate terminal and run this command, replacing `<server_ip>` with the server machine's IP address (e.g., `127.0.0.1` for localhost).

```shellscript
./client_app <server_ip>
```

After connecting, the client will display a main menu. Enter the number for the feature you want to use.

```plaintext
==================================================
              MINI ZOOM CLIENT
==================================================
1. Chat Mode (type '/exit' to return)
2. File Transfer
3. Video Streaming (ESC to return)
4. Voice Streaming (Ctrl+C to return)
5. Exit Application
==================================================
Choice:
```

  * **Chat Mode:** Type messages and press Enter. To return to the main menu, type `/exit`.
  * **File Transfer:** You will be prompted to enter the path to the file you want to send.
  * **Video Streaming:** Your webcam feed will be streamed. Press `ESC` to stop streaming and return to the main menu.
  * **Voice Streaming:** Your microphone input will be streamed. Press `Ctrl+C` to stop streaming and return to the main menu.

-----

## Architecture

The project is structured for modularity and maintainability.

```plaintext
mini-zoom/
├── app/
│   ├── client_main.cpp      # Main entry point for the client application
│   └── server_main.cpp      # Main entry point for the server application
├── client/
│   ├── chat_mode.h          # Client-side chat feature implementation
│   ├── client_common.h      # Common client constants and global declarations
│   ├── file_mode.h          # Client-side file transfer feature implementation
│   ├── video_mode.h         # Client-side video streaming feature implementation
│   └── voice_mode.h         # Client-side voice streaming feature implementation
├── server/
│   ├── chat_handler.h       # Server-side chat handling implementation
│   ├── file_handler.h       # Server-side file transfer handling implementation
│   ├── server_common.h      # Common server constants and global declarations
│   ├── tcp_server.h         # Main TCP server logic
│   ├── video_display.h      # Server-side video display loop (runs on main thread)
│   ├── video_handler.h      # Server-side video streaming handling implementation
│   └── voice_server.h       # Server-side UDP voice server implementation
├── utils/
│   ├── client_utils.h       # Client-specific utility functions (e.g., menu, non-blocking input)
│   ├── common_utils.cpp     # Implementation of shared utility functions
│   ├── common_utils.h       # Declarations for shared utility functions (e.g., logging, network helpers)
│   └── server_utils.h       # Server-specific utility functions (e.g., get client info)
└── README.md
```

-----

## Highlights

  * **Socket Programming:** TCP is used for reliable chat and file transfer, while UDP is used for low-latency voice streaming.
  * **Multimedia Streaming:** PortAudio is integrated for audio I/O, and OpenCV is used for video processing.
  * **Concurrency:** Multi-threading handles multiple clients and concurrent operations.
  * **Binary-safe Protocols:** The design handles arbitrary binary data for file transfers.
  * **Code Modularity:** Features are organized into separate header files for reusability and maintainability.

-----

## Demo

A `demo.mp4` walkthrough of all features is coming soon\!
