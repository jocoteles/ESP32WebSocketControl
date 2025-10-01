# ESP32 WebSocket Control & Data Streamer

This project demonstrates a robust ESP32 application for:
1.  Controlling ESP32 variables via WebSocket using JSON messages (Get/Set).
2.  Streaming real-time data (e.g., analog sensor readings) eficiente via WebSocket binary messages.
3.  Serving a web interface from the ESP32's LittleFS filesystem to interact with these functionalities.

The project is built using the **PlatformIO IDE** with the Arduino framework.

We also implemented a [pure WebBluetooth version](https://github.com/jocoteles/ESP32WebBluetooh) of this project. Please, check it out.

## Features

*   **WiFi Access Point Mode:** ESP32 hosts its own WiFi network.
*   **Static IP Configuration:** Assigns a static IP to the ESP32 in AP mode.
*   **Async Web Server & WebSockets:** Efficient, non-blocking communication.
*   **JSON Variable Control:** Remotely get and set pre-defined ESP32 variables.
*   **Binary Data Streaming:** Optimized for high-frequency data transfer.
*   **LittleFS File System:** Serves web application files (HTML, CSS, JS).
*   **Interactive LittleFS Management Utility:** A separate utility sketch is provided to format and manage the LittleFS partition.

## Project Structure

*   `src/main.cpp`: Main application sketch (`serverDemo`).
*   `lib/ESP32WebSocketLib/ESP32WebSocket.h`: Header file for the WebSocket library.
*   `lib/ESP32WebSocketLib/ESP32WebSocket.cpp`: Implementation of the WebSocket library.
*   `data/`: Contains the web application files (HTML, CSS, JavaScript) to be uploaded to LittleFS.
*   `utils/LittleFsManager/LittleFsManager.cpp`: Utility sketch for managing LittleFS.
*   `platformio.ini`: PlatformIO project configuration file.
*   `.vscode/tasks.json`: VS Code tasks for easier execution of PlatformIO commands (optional, but recommended).

## Prerequisites

*   [Visual Studio Code](https://code.visualstudio.com/)
*   [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) for VS Code
*   An ESP32 development board

## Setup & Usage

1.  **Clone the Repository:**
    ```bash
    git clone <your-repository-url>
    cd <repository-folder>
    ```
2.  **Open with PlatformIO:**
    *   Open VS Code.
    *   Click on the PlatformIO icon in the activity bar (left sidebar).
    *   Click "Open Project" and select the cloned project folder.
    *   PlatformIO will automatically install the required dependencies listed in `platformio.ini` (like ArduinoJson, ESPAsyncWebServer).

3.  **Configure `platformio.ini` (If Needed):**
    *   Verify the `board` type matches your ESP32 development board.
    *   Adjust WiFi SSID, password, and static IP in `src/main.cpp` if desired.

### A. Flashing the Main Application (`main_app` environment)

Ensure that `main_app` is the active PlatformIO environment.

1.  **Build the Firmware:**
    *   Use the shortcut `Ctrl+Alt+B` or the PlatformIO toolbar (✓ icon).

2.  **Upload the Firmware:**
    *   Use the shortcut `Ctrl+Alt+U` or the PlatformIO toolbar (→ icon).

3.  **Build & Upload Web Interface Files (LittleFS Data):**
    *   Ensure your web files (HTML, CSS, JS) are in the `data/` directory at the root of the project.
    *   **Using VS Code Tasks:**
        *   Open the Command Palette: `Ctrl+Shift+P`.
        *   Type `Tasks: Run Task`.
        *   Choose `PIO Upload LittleFS data folder`.
    *   **Using PlatformIO CLI in VS Code Terminal:**
        *   Open a new PlatformIO CLI terminal in VS Code (`Terminal > New Terminal` or PlatformIO sidebar).
        *   Run: `pio run --target buildfs --environment main_app`
        *   Then: `pio run --target uploadfs --environment main_app`
    *   **Using PlatformIO Project Tasks UI:**
        *   Click the PlatformIO icon in the VS Code activity bar.
        *   Navigate to `PROJECT TASKS > main_app > Filesystem`.
        *   Click `Build Filesystem Image`, then `Upload Filesystem Image`.
        *(Note: If these UI options are not visible, rely on the VS Code Tasks or CLI method).*

### B. (OPTIONAL) Managing LittleFS

If you need to format the LittleFS partition (e.g., on first use, or to completely clear it):

1.  **Build and Upload the Formatter Utility:**
    *   Open the Command Palette: `Ctrl+Shift+P`.
    *   Type `Tasks: Run Task`.
    *   Choose `Upload LittleFS Manager`.

2.  **Run the Formatter:**
    *   Open the Serial Monitor in PlatformIO. Set baud rate to `115200`.
    *   Follow the on-screen prompts in the Serial Monitor to format the LittleFS partition.

3.  **Re-flash Main Application:** If formatting, you **must** re-flash the main application firmware and re-upload the LittleFS data as described in section A.

## Connecting to the ESP32

1.  After flashing the main application and data, the ESP32 will start an Access Point.
2.  Connect your device (computer, phone) to the WiFi network with the SSID and password defined in `src/main.cpp` (default: `ESP32_Control_AP` / `password123`).
3.  Open a web browser and navigate to the ESP32's IP address (default: `192.168.5.1`).

## Troubleshooting

*   **"Upload Filesystem Image" task not visible in UI:** Use the VS Code custom tasks (via `Ctrl+Shift+P > Tasks: Run Task > PIO Upload LittleFS data folder`) or the PlatformIO CLI commands (`pio run -t uploadfs -e main_app`) in the VS Code terminal.
*   **Port not found:** Ensure your ESP32 is connected and the correct port is auto-detected or specified in `platformio.ini` (`upload_port = ...`).
*   **Upload errors:** Try holding the "BOOT" button on the ESP32 while initiating the upload, then release it.
