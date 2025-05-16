/**
 * @file ESP32WebSocket.cpp
 * @brief Implementation file for the ESP32WebSocket library.
 *        Handles WebSocket communication for ESP32 variable control and data streaming.
 */
#include "ESP32WebSocket.h"
#include <LittleFS.h> // FILESYSTEM to serve web page through html and js files

// --- Debug Configuration ---
// Uncomment the line below to enable verbose debug output from this library to the Serial monitor.
#define DEBUG_ESP32_WEBSOCKET_LIB

// --- Library-Internal Definitions ---

/**
 * @struct StaticFileConfig
 * @brief Defines the configuration for a single static file to be served by the HTTP server.
 *        Used internally by the library.
 */
struct StaticFileConfig {
  const char* path;         ///< HTTP route and also the path within the LittleFS filesystem (from its root).
  const char* contentType;  ///< HTTP Content-Type for the file.
};

// --- LIST OF STATIC FILES TO BE SERVED (DEFINED WITHIN THIS LIBRARY) ---
// Modify this list directly in the library for the web app files you intend to serve.
// The 'path' is the HTTP route and also the path within the LittleFS filesystem (from its root).
// Ensure these paths match how files are uploaded via "Sketch Data Upload" into the 'data/' directory.
static const StaticFileConfig libraryStaticFilesToServe[] = {
  // HTTP Route (and LittleFS Path)      Content Type
  { "/",                                "text/html" },                 // Root path will serve /index.html
  { "/index.html",                      "text/html" },
  { "/manifest.json",                   "application/manifest+json" }, // If using a manifest for "Add to Home Screen"
  { "/favicon.ico",                     "image/x-icon" },            // Standard favicon

  // JavaScript modules
  { "/js/main.js",                      "application/javascript" },
  { "/js/websocketService.js",          "application/javascript" },
  { "/js/uiUpdater.js",                 "application/javascript" },
  { "/js/appState.js",                  "application/javascript" },
  
  // CSS files
  { "/css/pico.min.css",                "text/css" },                 // If serving Pico.css locally
  { "/css/styles.css",                  "text/css" },                 // Other custom CSS

  // Icon files (add all sizes you reference in manifest.json or index.html)  
  { "/icons/icon-76x76.png",            "image/png" },
  { "/icons/icon-120x120.png",          "image/png" },
  { "/icons/icon-144x144.png",          "image/png" },
  { "/icons/icon-152x152.png",          "image/png" },
  { "/icons/icon-192x192.png",          "image/png" },
  { "/icons/icon-512x512.png",          "image/png" },
  // Add more entries for other icons or static assets as needed
};
// Calculate the number of static files to serve
static const size_t numLibraryStaticFilesToServe = sizeof(libraryStaticFilesToServe) / sizeof(libraryStaticFilesToServe[0]);


// --- Library-Internal Objects and State ---

// Async Web Server object listening on standard HTTP port 80
static AsyncWebServer server(80); 
// Async WebSocket object handling connections on the "/ws" endpoint
static AsyncWebSocket ws("/ws");   

// Pointers to the application's variable array and its size (provided during init)
static VariableConfig* _variables = nullptr; 
static int _numVariables = 0;

// Pointers to the application's stream control callback functions
static StreamControlCallback _onStreamStartCallback = nullptr;
static StreamControlCallback _onStreamStopCallback = nullptr;
// Internal flag to track if data streaming is currently active
static bool _isStreaming = false; 

// --- Internal Helper Function Prototypes ---
static int findVariableIndexInternal(const char* name);
static bool setVariableValueInternal(int index, JsonVariant newValueVariant);
static void sendVariableValueInternal(uint32_t clientId, int variableIndex);
static void sendStatusInternal(uint32_t clientId, const char* status, const char* message);
static const char* varTypeToCharString(VarType type); // Converts VarType enum to string
static void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);


// --- Internal Helper Function Implementations ---

/**
 * @brief Finds the index of a variable in the _variables array by its name.
 * @param name The name of the variable to find.
 * @return The index in the _variables array, or -1 if not found or not initialized.
 */
static int findVariableIndexInternal(const char* name) {
  if (!_variables || _numVariables <= 0 || !name) return -1; 
  for (int i = 0; i < _numVariables; i++) {
    if (strcmp(name, _variables[i].name) == 0) {
      return i; 
    }
  }
  return -1; 
}

/**
 * @brief Attempts to set a variable's value, performing type and range checks.
 * @param index The index of the variable in the _variables array.
 * @param newValueVariant A JsonVariant containing the value received from the client.
 * @return True if the value was set successfully, false otherwise.
 */
static bool setVariableValueInternal(int index, JsonVariant newValueVariant) {
  if (!_variables || index < 0 || index >= _numVariables) {
    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
    Serial.println(F("[ESP32WS] setVariableValueInternal: Invalid index or uninitialized variables."));
    #endif
    return false; 
  }
  VariableConfig& var = _variables[index]; 
  switch (var.type) {
    case TYPE_INT:
      if (!newValueVariant.is<int>() && !(newValueVariant.is<float>() && newValueVariant.as<float>() == (int)newValueVariant.as<float>() ) ) {
        #ifdef DEBUG_ESP32_WEBSOCKET_LIB
        Serial.printf("[ESP32WS] Set Error: Value for '%s' is not a compatible integer.\n", var.name);
        #endif
        return false;
      }
      { 
        int newValue = newValueVariant.as<int>();
        if (var.hasLimits && (static_cast<double>(newValue) < var.minVal || static_cast<double>(newValue) > var.maxVal)) {
          #ifdef DEBUG_ESP32_WEBSOCKET_LIB
          Serial.printf("[ESP32WS] Set Error: Value %d for '%s' is outside limits [%.2f, %.2f].\n", newValue, var.name, var.minVal, var.maxVal);
          #endif
          return false;
        }
        var.intValue = newValue;
        #ifdef DEBUG_ESP32_WEBSOCKET_LIB
        Serial.printf("[ESP32WS] Set OK: Variable '%s' (int) updated to %d.\n", var.name, var.intValue);
        #endif
      }
      break; 
    case TYPE_FLOAT:
      if (!newValueVariant.is<float>() && !newValueVariant.is<int>()) {
        #ifdef DEBUG_ESP32_WEBSOCKET_LIB
        Serial.printf("[ESP32WS] Set Error: Value for '%s' is not a float/number.\n", var.name);
        #endif
        return false;
      }
      { 
        float newValue = newValueVariant.as<float>();
        if (var.hasLimits && (static_cast<double>(newValue) < var.minVal || static_cast<double>(newValue) > var.maxVal)) {
           #ifdef DEBUG_ESP32_WEBSOCKET_LIB
           Serial.printf("[ESP32WS] Set Error: Value %.3f for '%s' is outside limits [%.2f, %.2f].\n", newValue, var.name, var.minVal, var.maxVal);
           #endif
           return false;
        }
        var.floatValue = newValue;
        #ifdef DEBUG_ESP32_WEBSOCKET_LIB
        Serial.printf("[ESP32WS] Set OK: Variable '%s' (float) updated to %.3f.\n", var.name, var.floatValue);
        #endif
      }
      break; 
    case TYPE_STRING:
      if (!newValueVariant.is<const char*>() && !newValueVariant.is<String>()) { 
        #ifdef DEBUG_ESP32_WEBSOCKET_LIB
        Serial.printf("[ESP32WS] Set Error: Value for '%s' is not a string.\n", var.name);
        #endif
        return false;
      }
      var.stringValue = newValueVariant.as<String>(); 
      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
      Serial.printf("[ESP32WS] Set OK: Variable '%s' (string) updated to '%s'.\n", var.name, var.stringValue.c_str());
      #endif
      break; 
    default:
      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
      Serial.printf("[ESP32WS] Set Error: Unknown internal type for variable '%s'.\n", var.name);
      #endif
      return false; 
  }
  return true; 
}

/**
 * @brief Sends the current value of a variable as JSON to a specific client.
 * @param clientId The ID of the target WebSocket client.
 * @param variableIndex The index of the variable in the _variables array.
 */
static void sendVariableValueInternal(uint32_t clientId, int variableIndex) {
   if (!_variables || variableIndex < 0 || variableIndex >= _numVariables) return;
    StaticJsonDocument<256> jsonDoc; // Adjust size if needed for long names/strings
    VariableConfig& var = _variables[variableIndex];
    jsonDoc["variable"] = var.name;
    switch (var.type) {
      case TYPE_INT:    jsonDoc["value"] = var.intValue;    break;
      case TYPE_FLOAT:  jsonDoc["value"] = var.floatValue;  break;
      case TYPE_STRING: jsonDoc["value"] = var.stringValue; break;
      default:          jsonDoc["value"] = nullptr; jsonDoc["error"] = "Unknown internal type"; break; 
    }
    String response;
    serializeJson(jsonDoc, response);
    ws.text(clientId, response);
    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
    // Serial.printf("[ESP32WS] Sent Value to #%u: %s\n", clientId, response.c_str()); 
    #endif
}

/**
 * @brief Sends a status or error message as JSON to a specific client.
 * @param clientId The ID of the target WebSocket client.
 * @param status A string indicating the status (e.g., "ok", "error", "info").
 * @param message A descriptive message detailing the status or error.
 */
static void sendStatusInternal(uint32_t clientId, const char* status, const char* message) {
    StaticJsonDocument<192> jsonDoc; // Adjusted size, ensure it's enough for status + message
    jsonDoc["status"] = status;
    jsonDoc["message"] = message;
    String response;
    serializeJson(jsonDoc, response);
    ws.text(clientId, response);
    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
    Serial.printf("[ESP32WS] Sent Status to #%u: %s\n", clientId, response.c_str()); 
    #endif
}

/**
 * @brief Converts VarType enum to its string representation.
 * @param type The VarType enum value.
 * @return A const char* string (e.g., "INT", "FLOAT", "STRING").
 */
static const char* varTypeToCharString(VarType type) {
    switch (type) {
        case TYPE_INT:    return "INT";
        case TYPE_FLOAT:  return "FLOAT";
        case TYPE_STRING: return "STRING";
        default:          return "UNKNOWN";
    }
}

// --- Main WebSocket Event Handler ---

/**
 * @brief Callback function invoked by the AsyncWebSocket library for various events.
 */
static void onWebSocketEvent(AsyncWebSocket *serverArg, AsyncWebSocketClient *client, AwsEventType type,
                             void *arg, uint8_t *data, size_t len) {
  
  switch (type) {
    case WS_EVT_CONNECT:
      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
      Serial.printf("[ESP32WS] WebSocket Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      #endif
      break;

    case WS_EVT_DISCONNECT:
      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
      Serial.printf("[ESP32WS] WebSocket Client #%u disconnected\n", client->id());
      #endif
      if (_isStreaming && ws.count() == 0 && _onStreamStopCallback != nullptr) { // ws.count() is now 0 AFTER this client is removed
           #ifdef DEBUG_ESP32_WEBSOCKET_LIB
           Serial.println(F("[ESP32WS] Last client disconnected. Auto-stopping stream."));
           #endif
           _onStreamStopCallback(); 
           _isStreaming = false;    
      }
      break;

    case WS_EVT_DATA:
      { 
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        
        if (info->opcode == WS_TEXT && info->final && info->index == 0 && info->len == len) {
          // data[len] = 0; // Risky, can cause buffer overflow. Use deserializeJson with length.
          #ifdef DEBUG_ESP32_WEBSOCKET_LIB
          // Print raw data carefully, ensuring it's null-terminated for printing if needed
          // Or print byte by byte if not guaranteed to be text.
          // For JSON, deserializeJson will handle it.
          // char tempBuf[len + 1];
          // memcpy(tempBuf, data, len);
          // tempBuf[len] = '\0';
          // Serial.printf("[ESP32WS] Received Text from #%u: %s\n", client->id(), tempBuf);
          Serial.printf("[ESP32WS] Received Text from #%u (%d bytes)\n", client->id(), len);
          #endif

          StaticJsonDocument<256> jsonDoc; // Adjust size for expected command complexity
          DeserializationError error = deserializeJson(jsonDoc, (const char*)data, len); // Use (const char*) and len

          if (error) {
            Serial.printf("[ESP32WS] JSON Parse Error: %s\n", error.c_str()); // Always log parse errors
            sendStatusInternal(client->id(), "error", "Invalid JSON format received.");
            return; 
          }

          const char* action = jsonDoc["action"];
          if (!action) {
             #ifdef DEBUG_ESP32_WEBSOCKET_LIB
             Serial.println(F("[ESP32WS] JSON missing 'action' field."));
             #endif
             sendStatusInternal(client->id(), "error", "JSON missing 'action' field."); return;
          }

          if (strcmp(action, "get") == 0 || strcmp(action, "set") == 0) {
              const char* variableName = jsonDoc["variable"];
              if (!variableName) { 
                #ifdef DEBUG_ESP32_WEBSOCKET_LIB
                Serial.println(F("[ESP32WS] Missing 'variable' field for get/set action."));
                #endif
                sendStatusInternal(client->id(), "error", "Missing 'variable' field for get/set action."); return; 
              }
              int varIndex = findVariableIndexInternal(variableName);
              if (varIndex == -1) { 
                #ifdef DEBUG_ESP32_WEBSOCKET_LIB
                Serial.printf("[ESP32WS] Variable name '%s' not found.\n", variableName);
                #endif
                sendStatusInternal(client->id(), "error", "Variable name not found."); return; 
              }

              if (strcmp(action, "get") == 0) {
                  sendVariableValueInternal(client->id(), varIndex); 
              } else { // action == "set"
                  if (!jsonDoc.containsKey("value") || jsonDoc["value"].isNull()) { 
                    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
                    Serial.println(F("[ESP32WS] Missing or null 'value' field for set action."));
                    #endif
                    sendStatusInternal(client->id(), "error", "Missing or null 'value' field for set action."); return; 
                  }
                  JsonVariant newValueVariant = jsonDoc["value"];
                  if (setVariableValueInternal(varIndex, newValueVariant)) {
                      sendVariableValueInternal(client->id(), varIndex); // Send back the updated value
                      // Optionally, broadcast to all clients if variable was set by one client
                      // broadcastVariableUpdate(variableName); 
                  } else {
                      // Error message already printed by setVariableValueInternal if DEBUG is on
                      sendStatusInternal(client->id(), "error", "Failed to set value (invalid type or out of limits).");
                  }
              }
          } 
          else if (strcmp(action, "start_stream") == 0) {
              if (_onStreamStartCallback != nullptr) {
                  if (!_isStreaming) {
                      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
                      Serial.println(F("[ESP32WS] Action: start_stream - Calling app callback."));
                      #endif
                      _onStreamStartCallback(); 
                      _isStreaming = true;      
                      sendStatusInternal(client->id(), "ok", "Stream started.");
                  } else { 
                    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
                    Serial.println(F("[ESP32WS] Info: Stream was already active."));
                    #endif
                    sendStatusInternal(client->id(), "info", "Stream was already active."); 
                  }
              } else {
                  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
                  Serial.println(F("[ESP32WS] Action: start_stream - No callback registered."));
                  #endif
                  sendStatusInternal(client->id(), "error", "Streaming feature not implemented/configured.");
              }
          } 
          else if (strcmp(action, "stop_stream") == 0) {
               if (_onStreamStopCallback != nullptr) {
                  if (_isStreaming) {
                      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
                      Serial.println(F("[ESP32WS] Action: stop_stream - Calling app callback."));
                      #endif
                      _onStreamStopCallback(); 
                      _isStreaming = false;     
                      sendStatusInternal(client->id(), "ok", "Stream stopped.");
                  } else { 
                    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
                    Serial.println(F("[ESP32WS] Info: Stream was already stopped."));
                    #endif
                    sendStatusInternal(client->id(), "info", "Stream was already stopped."); 
                  }
              } else {
                  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
                  Serial.println(F("[ESP32WS] Action: stop_stream - No callback registered."));
                  #endif
                  sendStatusInternal(client->id(), "error", "Streaming feature not implemented/configured.");
              }
          }
          else if (strcmp(action, "get_all_vars_config") == 0) {
              #ifdef DEBUG_ESP32_WEBSOCKET_LIB
              Serial.printf("[ESP32WS] Action: get_all_vars_config received from #%u\n", client->id());
              #endif
              if (!_variables || _numVariables <= 0) {
                  sendStatusInternal(client->id(), "error", "No variables configured on server.");
                  return;
              }
              
              const int avgBytesPerVar = 100; // Conservative estimate
              DynamicJsonDocument responseDoc(_numVariables * avgBytesPerVar + 128); 

              responseDoc["status"] = "var_config_list";
              JsonArray varsArray = responseDoc.createNestedArray("variables");

              for (int i = 0; i < _numVariables; i++) {
                  JsonObject varObj = varsArray.createNestedObject();
                  varObj["name"] = _variables[i].name;
                  varObj["type"] = varTypeToCharString(_variables[i].type);
                  
                  switch (_variables[i].type) {
                      case TYPE_INT:    varObj["value"] = _variables[i].intValue;    break;
                      case TYPE_FLOAT:  varObj["value"] = _variables[i].floatValue;  break;
                      case TYPE_STRING: varObj["value"] = _variables[i].stringValue; break;
                  }
                  varObj["hasLimits"] = _variables[i].hasLimits;
                  if (_variables[i].hasLimits) {
                      varObj["min"] = _variables[i].minVal;
                      varObj["max"] = _variables[i].maxVal;
                  }
              }
              String response;
              serializeJson(responseDoc, response);
              ws.text(client->id(), response);
              #ifdef DEBUG_ESP32_WEBSOCKET_LIB
              Serial.println(F("[ESP32WS] Sent var_config_list to client."));
              #endif
          }
          else {
              #ifdef DEBUG_ESP32_WEBSOCKET_LIB
              Serial.printf("[ESP32WS] Unknown action received: %s\n", action);
              #endif
              sendStatusInternal(client->id(), "error", "Unknown 'action' command.");
          }
        } 
        else if (info->opcode == WS_BINARY) {
            #ifdef DEBUG_ESP32_WEBSOCKET_LIB
            Serial.printf("[ESP32WS] Received Binary from #%u: %u bytes (ignored by library)\n", client->id(), len);
            #endif
        }
      } 
      break;

    case WS_EVT_PONG: 
      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
      // Serial.printf("[ESP32WS] WebSocket Pong received from #%u\n", client->id()); 
      #endif
      break;

    case WS_EVT_ERROR: // This event gives limited info, usually just an error code
      // arg is a uint16_t* (error code), data is a char* (error message)
      Serial.printf("[ESP32WS] WebSocket Client #%u error #%u: %s\n", client->id(), *((uint16_t*)arg), (char*)data);
      break;
  } 
}

// --- Public Function Implementations ---

void initWiFiWebSocketServer(const char *ssid, const char *password,
                              const uint8_t staticIpParam[4],
                              VariableConfig *appVariables, int appNumVariables,
                              ArRequestHandlerFunction customNotFoundHandler) { // Renamed for clarity

  _variables = appVariables;
  _numVariables = appNumVariables;

  Serial.println(F("\n--- [ESP32WS] initWiFiWebSocketServer: START ---")); 

  // Validate variable configuration
  if (appNumVariables < 0) {
      Serial.println(F("[ESP32WS] CRITICAL ERROR: appNumVariables cannot be negative."));
      return;
  }
  if (appNumVariables > 0 && !appVariables) { 
      Serial.println(F("[ESP32WS] CRITICAL ERROR: appVariables is NULL but appNumVariables is greater than 0."));
      return; 
  }
  if (appNumVariables == 0) {
      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
      Serial.println(F("[ESP32WS] Info: Initializing without application variables (appNumVariables is 0)."));
      #endif
  } else {
      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
      Serial.println(F("[ESP32WS] Variable array parameters check OK."));
      #endif
  }


  // --- Initialize LittleFS ---
  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
  Serial.println(F("[ESP32WS] Initializing LittleFS..."));
  #endif
  if (!LittleFS.begin(true)) { // true = format if mount failed
      Serial.println(F("[ESP32WS] CRITICAL ERROR: LittleFS Mount Failed! Unable to proceed."));
      Serial.println(F("[ESP32WS] --> Please ensure LittleFS is correctly formatted and data uploaded."));
      return; 
  }
  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
  Serial.println(F("[ESP32WS] LittleFS mounted successfully."));
  #endif

  // Reset WiFi state for a cleaner start
  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
  Serial.println(F("[ESP32WS] Attempting to reset WiFi state..."));
  #endif
  WiFi.persistent(false); 
  WiFi.disconnect(true);  
  WiFi.mode(WIFI_OFF);    
  delay(100);             
  WiFi.mode(WIFI_AP);     
  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
  Serial.println(F("[ESP32WS] WiFi state reset, AP mode set."));
  #endif
  
  // Configure Static IP for the Access Point if provided
  if (staticIpParam != nullptr) {
    IPAddress apIP(staticIpParam[0], staticIpParam[1], staticIpParam[2], staticIpParam[3]);
    IPAddress gatewayIP(staticIpParam[0], staticIpParam[1], staticIpParam[2], staticIpParam[3]);
    IPAddress subnetMask(255, 255, 255, 0); 

    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
    Serial.printf("[ESP32WS] Attempting to configure static AP IP: %s\n", apIP.toString().c_str());
    #endif
    if (!WiFi.softAPConfig(apIP, gatewayIP, subnetMask)) {
      Serial.println(F("[ESP32WS] ERROR: Failed to configure static AP IP address! Will use default."));
    } else {
      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
      Serial.println(F("[ESP32WS] Static AP IP configuration successful."));
      #endif
    }
  } else {
    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
    Serial.println(F("[ESP32WS] Info: No static IP provided. Using default AP IP (typically 192.168.4.1)."));
    #endif
  }

  // Start the WiFi Access Point
  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
  Serial.printf("[ESP32WS] Starting WiFi Access Point (SSID: %s)...\n", ssid);
  #endif
  bool apStarted = WiFi.softAP(ssid, password); 

  if (apStarted) {
      Serial.print(F("[ESP32WS] Access Point started. IP Address: ")); 
      Serial.println(WiFi.softAPIP()); 
      // Additional check if static IP was intended but not achieved
      if (staticIpParam != nullptr && WiFi.softAPIP() != IPAddress(staticIpParam[0], staticIpParam[1], staticIpParam[2], staticIpParam[3])) {
          if (WiFi.softAPIP() == IPAddress(0,0,0,0)) {
             Serial.println(F("[ESP32WS] WARNING: AP IP is 0.0.0.0! AP may not be fully functional."));
          } else if (WiFi.softAPIP() == IPAddress(192,168,4,1) && (staticIpParam[0]!=192 || staticIpParam[1]!=168 || staticIpParam[2]!=4 || staticIpParam[3]!=1)) {
             Serial.println(F("[ESP32WS] WARNING: Actual AP IP is the default (192.168.4.1), not the configured static IP. softAPConfig might have failed silently or been overridden."));
          } else {
             Serial.println(F("[ESP32WS] WARNING: Actual AP IP does not match configured static IP. Check for conflicts."));
          }
      }
  } else {
      Serial.println(F("[ESP32WS] CRITICAL ERROR: Failed to start Access Point!"));
      return; 
  }

  // Configure WebSocket Server
  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
  Serial.println(F("[ESP32WS] Configuring WebSocket server..."));
  #endif
  ws.onEvent(onWebSocketEvent); 
  server.addHandler(&ws);
  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
  Serial.println(F("[ESP32WS] WebSocket handler attached to /ws endpoint.")); 
  #endif

  // Configure HTTP Server to Serve Static Files from LittleFS using the internal list
  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
  Serial.println(F("[ESP32WS] Configuring HTTP server for static files from internal library list..."));
  #endif
  if (numLibraryStaticFilesToServe > 0) {
    for (size_t i = 0; i < numLibraryStaticFilesToServe; i++) {
      const char* path_capture = libraryStaticFilesToServe[i].path;
      const char* contentType_capture = libraryStaticFilesToServe[i].contentType;

      if (strcmp(path_capture, "/") == 0) {
          server.on("/", HTTP_GET, [contentType_capture](AsyncWebServerRequest *request){ 
              String fsPath = "/index.html"; 
              if (LittleFS.exists(fsPath)) {
                  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
                  Serial.printf("[ESP32WS] HTTP GET: /, serving %s as %s\n", fsPath.c_str(), contentType_capture);
                  #endif
                  request->send(LittleFS, fsPath, contentType_capture);
              } else {
                  Serial.printf("[ESP32WS] HTTP GET: /, File %s NOT FOUND in LittleFS\n", fsPath.c_str());
                  request->send(404, "text/plain", "/index.html Not Found in LittleFS");
              }
          });
          #ifdef DEBUG_ESP32_WEBSOCKET_LIB
          Serial.printf("[ESP32WS] Registered handler for: / (serving /index.html, Content-Type: %s)\n", contentType_capture);
          #endif
      } else {
          server.on(path_capture, HTTP_GET, [path_capture, contentType_capture](AsyncWebServerRequest *request){
              String fsPath = String(path_capture); 
              if (LittleFS.exists(fsPath)) {
                  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
                  Serial.printf("[ESP32WS] HTTP GET: %s, serving %s as %s\n", request->url().c_str(), fsPath.c_str(), contentType_capture);
                  #endif
                  request->send(LittleFS, fsPath, contentType_capture);
              } else {
                  Serial.printf("[ESP32WS] HTTP GET: %s, File %s NOT FOUND in LittleFS\n", request->url().c_str(), fsPath.c_str());
                  request->send(404, "text/plain", "File Not Found in LittleFS");
              }
          });
          #ifdef DEBUG_ESP32_WEBSOCKET_LIB
          Serial.printf("[ESP32WS] Registered handler for: %s (Content-Type: %s)\n", path_capture, contentType_capture);
          #endif
      }
    }
  } else {
    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
    Serial.println(F("[ESP32WS] No static files defined in libraryStaticFilesToServe array. Serving default root message."));
    #endif
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "ESP32 Server Active. No index.html configured in library's file list.");
    });
  }

  // Configure Not Found Handler
  if (customNotFoundHandler) {
      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
      Serial.println(F("[ESP32WS] Registering custom Not Found handler."));
      #endif
      server.onNotFound(customNotFoundHandler);
  } else {
      #ifdef DEBUG_ESP32_WEBSOCKET_LIB
      Serial.println(F("[ESP32WS] Registering library's default Not Found handler."));
      #endif
      server.onNotFound([](AsyncWebServerRequest *request){
          #ifdef DEBUG_ESP32_WEBSOCKET_LIB
          Serial.printf("[ESP32WS] Not Found: HTTP %s: %s\n", request->methodToString(), request->url().c_str());
          #endif
          request->send(404, "text/plain", "Error 404: Resource Not Found");
      });
  }

  // Start the Web Server
  #ifdef DEBUG_ESP32_WEBSOCKET_LIB
  Serial.println(F("[ESP32WS] Starting HTTP server (server.begin())..."));
  #endif
  server.begin();
  Serial.println(F("[ESP32WS] HTTP & WebSocket Server started."));
  Serial.println(F("--- [ESP32WS] initWiFiWebSocketServer: COMPLETE ---")); 
}

/**
 * @brief Registers application callbacks for stream control commands.
 */
void setStreamCallbacks(StreamControlCallback onStart, StreamControlCallback onStop) {
    _onStreamStartCallback = onStart;
    _onStreamStopCallback = onStop;
    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
    Serial.println(F("[ESP32WS] Stream control callbacks registered."));
    #endif
}

/**
 * @brief Broadcasts a variable update (JSON) to all connected clients.
 */
void broadcastVariableUpdate(const char* variableName) {
    if (ws.count() == 0) return; 
    if (!_variables || _numVariables <= 0) { // Ensure variables are configured
        #ifdef DEBUG_ESP32_WEBSOCKET_LIB
        Serial.println(F("[ESP32WS] Broadcast Error: No variables configured to broadcast."));
        #endif
        return;
    }
    int index = findVariableIndexInternal(variableName);
    if (index < 0) {
        #ifdef DEBUG_ESP32_WEBSOCKET_LIB
        Serial.printf("[ESP32WS] Broadcast Error: Variable '%s' not found.\n", variableName);
        #endif
        return;
    }
    StaticJsonDocument<256> jsonDoc; // Ensure size is adequate
    VariableConfig& var = _variables[index];
    jsonDoc["variable"] = var.name;
    switch (var.type) {
        case TYPE_INT:    jsonDoc["value"] = var.intValue;    break;
        case TYPE_FLOAT:  jsonDoc["value"] = var.floatValue;  break;
        case TYPE_STRING: jsonDoc["value"] = var.stringValue; break;
        default: 
            #ifdef DEBUG_ESP32_WEBSOCKET_LIB
            Serial.printf("[ESP32WS] Broadcast Error: Unknown type for var '%s'\n", variableName);
            #endif
            return; 
    }
    String response;
    serializeJson(jsonDoc, response);
    ws.textAll(response);
    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
    // Serial.printf("[ESP32WS] Broadcast Sent: %s\n", response.c_str());
    #endif
}

/**
 * @brief Broadcasts binary data to all connected clients.
 */
void broadcastBinaryData(const uint8_t* data, size_t len) {
    if (ws.count() > 0 && data != nullptr && len > 0) {
        ws.binaryAll(const_cast<uint8_t*>(data), len);
    } 
}

/**
 * @brief Cleans up disconnected clients.
 *        This is typically managed by the AsyncWebServer library, but can be called manually.
 */
void cleanupWebSocketClients() {
    ws.cleanupClients();
    #ifdef DEBUG_ESP32_WEBSOCKET_LIB
    // Serial.println(F("[ESP32WS] WebSocket clients cleanup requested."));
    #endif
}