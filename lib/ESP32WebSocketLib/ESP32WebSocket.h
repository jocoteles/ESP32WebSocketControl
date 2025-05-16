/**
 * @file ESP32WebSocket.h
 * @brief Header file for a library to manage WebSocket communication 
 *        for controlling ESP32 variables and streaming data.
 *        Handles WiFi AP setup, WebSocket server, JSON message parsing (get/set),
 *        and binary data streaming control.
 */
#ifndef ESP32_WEBSOCKET_H
#define ESP32_WEBSOCKET_H

#include <Arduino.h>            // Base Arduino types (String, etc.)
#include <WiFi.h>               // WiFi functions and types (IPAddress)
#include <ESPAsyncWebServer.h>  // Async Web Server and WebSocket types (requires AsyncTCP dependency)
#include <ArduinoJson.h>        // JSON handling library

// --- Supported Variable Types ---

/**
 * @enum VarType
 * @brief Defines the supported data types for variables managed via WebSocket.
 */
enum VarType {
  TYPE_INT,    ///< Integer type
  TYPE_FLOAT,  ///< Floating-point type (ESP32 typically uses 32-bit float)
  TYPE_STRING  ///< String type (using Arduino String class)
};

// --- Variable Configuration Structure ---

/**
 * @struct VariableConfig
 * @brief Defines the structure for a single configurable variable.
 *        The application (.ino file) will create an array of these.
 */
struct VariableConfig {
  const char* name;     ///< Unique name used in JSON communication (e.g., "led_intensity").
  VarType type;         ///< Data type of the variable (TYPE_INT, TYPE_FLOAT, TYPE_STRING).

  // --- Value Storage ---
  // Note: Only one of these fields will hold the active value, based on 'type'.
  int intValue;         ///< Storage for integer values.
  float floatValue;       ///< Storage for float values.
  String stringValue;     ///< Storage for string values.

  // --- Optional Validation Limits (for numeric types) ---
  double minVal;        ///< Minimum allowed value (use double for flexibility).
  double maxVal;        ///< Maximum allowed value (use double for flexibility).
  bool hasLimits;       ///< True if minVal/maxVal validation should be applied.
};

// --- Stream Control Callback Type ---

/**
 * @typedef StreamControlCallback
 * @brief Defines the function pointer type for stream start/stop callbacks.
 *        These functions (defined in the application .ino) take no arguments.
 */
typedef void (*StreamControlCallback)();

// --- Public Library Functions ---

/**
 * @brief Initializes WiFi in Access Point mode with a static IP, 
 *        starts the AsyncWebServer, and sets up the WebSocket endpoint ("/ws").
 * 
 * @param ssid The desired network name (SSID) for the Access Point.
 * @param password The password for the Access Point (8+ characters recommended, or nullptr for an open network).
 * @param staticIp Desired static IP as 4 octets (e.g., {192, 168, 5, 1})
 * @param appVariables Pointer to the application's array of VariableConfig structs.
 * @param appNumVariables The total number of elements in the appVariables array.
 * @param defaultRouteHandler (Optional) A callback function (of type ArRequestHandlerFunction) 
 *                            to handle HTTP GET requests to the root "/" path. 
 *                            If nullptr, a default "Server Active" message is sent.
 *                            ArRequestHandlerFunction is defined in ESPAsyncWebServer.h.
 */
void initWiFiWebSocketServer(
    const char *ssid, 
    const char *password, 
    const uint8_t staticIp[4],
    VariableConfig *appVariables,
    int appNumVariables,
    ArRequestHandlerFunction defaultRouteHandler = nullptr
);

/**
 * @brief Registers the application-defined functions to be called when 
 *        "start_stream" or "stop_stream" commands are received via WebSocket.
 * 
 * @param onStart Pointer to the function in the .ino file to call when streaming should start.
 * @param onStop Pointer to the function in the .ino file to call when streaming should stop.
 */
void setStreamCallbacks(StreamControlCallback onStart, StreamControlCallback onStop);

/**
 * @brief Sends the current value of a specified variable as a JSON message
 *        to ALL currently connected WebSocket clients.
 *        Useful for notifying clients of changes initiated by the ESP32 itself (e.g., sensor readings).
 * 
 * @param variableName The 'name' field of the VariableConfig whose value should be broadcast.
 */
void broadcastVariableUpdate(const char* variableName);

/**
 * @brief Sends a block of raw binary data to ALL currently connected WebSocket clients.
 *        This is intended for high-frequency data streaming.
 * 
 * @param data Pointer to the buffer containing the binary data to send.
 * @param len The size of the data buffer in bytes.
 */
void broadcastBinaryData(const uint8_t* data, size_t len);

/**
 * @brief Performs cleanup of disconnected WebSocket clients.
 *        Generally managed automatically by the underlying library, but can be called
 *        manually if needed (e.g., periodically in the main loop if experiencing issues).
 */
void cleanupWebSocketClients();

#endif // ESP32_WEBSOCKET_H