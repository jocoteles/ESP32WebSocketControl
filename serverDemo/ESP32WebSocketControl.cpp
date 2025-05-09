/**
 * @file ESP32WebSocketControl.cpp
 * @brief Implementation file for the ESP32WebSocketControl library.
 *        Handles WebSocket communication for ESP32 variable control and data streaming.
 */
#include "ESP32WebSocketControl.h"

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
  if (!_variables || !name) return -1; 
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
    Serial.println("setVariableValueInternal: Invalid index or uninitialized variables.");
    return false; 
  }
  VariableConfig& var = _variables[index]; 
  switch (var.type) {
    case TYPE_INT:
      if (!newValueVariant.is<int>() && !(newValueVariant.is<float>() && newValueVariant.as<float>() == (int)newValueVariant.as<float>() ) ) { // Allow float if it's a whole number
        Serial.printf("Set Error: Value for '%s' is not a compatible integer.\n", var.name); return false;
      }
      { 
        int newValue = newValueVariant.as<int>();
        if (var.hasLimits && (static_cast<double>(newValue) < var.minVal || static_cast<double>(newValue) > var.maxVal)) {
          Serial.printf("Set Error: Value %d for '%s' is outside limits [%.2f, %.2f].\n", newValue, var.name, var.minVal, var.maxVal); return false;
        }
        var.intValue = newValue;
        Serial.printf("Set OK: Variable '%s' (int) updated to %d.\n", var.name, var.intValue);
      }
      break; 
    case TYPE_FLOAT:
      if (!newValueVariant.is<float>() && !newValueVariant.is<int>()) {
        Serial.printf("Set Error: Value for '%s' is not a float/number.\n", var.name); return false;
      }
      { 
        float newValue = newValueVariant.as<float>();
        if (var.hasLimits && (static_cast<double>(newValue) < var.minVal || static_cast<double>(newValue) > var.maxVal)) {
           Serial.printf("Set Error: Value %.3f for '%s' is outside limits [%.2f, %.2f].\n", newValue, var.name, var.minVal, var.maxVal); return false;
        }
        var.floatValue = newValue;
        Serial.printf("Set OK: Variable '%s' (float) updated to %.3f.\n", var.name, var.floatValue);
      }
      break; 
    case TYPE_STRING:
      if (!newValueVariant.is<const char*>()) { 
        Serial.printf("Set Error: Value for '%s' is not a string.\n", var.name); return false;
      }
      var.stringValue = newValueVariant.as<String>(); 
      Serial.printf("Set OK: Variable '%s' (string) updated to '%s'.\n", var.name, var.stringValue.c_str());
      break; 
    default:
      Serial.printf("Set Error: Unknown internal type for variable '%s'.\n", var.name); return false; 
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
    // Serial.printf("Sent Value to #%u: %s\n", clientId, response.c_str()); 
}

/**
 * @brief Sends a status or error message as JSON to a specific client.
 * @param clientId The ID of the target WebSocket client.
 * @param status A string indicating the status (e.g., "ok", "error", "info").
 * @param message A descriptive message detailing the status or error.
 */
static void sendStatusInternal(uint32_t clientId, const char* status, const char* message) {
    StaticJsonDocument<128> jsonDoc; // Adjust size if status messages can be long
    jsonDoc["status"] = status;
    jsonDoc["message"] = message;
    String response;
    serializeJson(jsonDoc, response);
    ws.text(clientId, response);
    Serial.printf("Sent Status to #%u: %s\n", clientId, response.c_str()); 
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
static void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                             void *arg, uint8_t *data, size_t len) {
  
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket Client #%u disconnected\n", client->id());
      if (_isStreaming && ws.count() == 0 && _onStreamStopCallback != nullptr) {
           Serial.println("Last client disconnected. Auto-stopping stream.");
           _onStreamStopCallback(); 
           _isStreaming = false;    
      }
      break;

    case WS_EVT_DATA:
      { 
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        
        if (info->opcode == WS_TEXT && info->final && info->index == 0 && info->len == len) {
          data[len] = 0; 
          Serial.printf("Received Text from #%u: %s\n", client->id(), (char*)data);

          StaticJsonDocument<256> jsonDoc; // Adjust size for expected command complexity
          DeserializationError error = deserializeJson(jsonDoc, (char*)data);

          if (error) {
            Serial.printf("JSON Parse Error: %s\n", error.c_str());
            sendStatusInternal(client->id(), "error", "Invalid JSON format received.");
            return; 
          }

          const char* action = jsonDoc["action"];
          if (!action) {
             sendStatusInternal(client->id(), "error", "JSON missing 'action' field."); return;
          }

          if (strcmp(action, "get") == 0 || strcmp(action, "set") == 0) {
              const char* variableName = jsonDoc["variable"];
              if (!variableName) { sendStatusInternal(client->id(), "error", "Missing 'variable' field for get/set action."); return; }
              int varIndex = findVariableIndexInternal(variableName);
              if (varIndex == -1) { sendStatusInternal(client->id(), "error", "Variable name not found."); return; }
              if (strcmp(action, "get") == 0) {
                  sendVariableValueInternal(client->id(), varIndex); 
              } else { // action == "set"
                  if (!jsonDoc.containsKey("value") || jsonDoc["value"].isNull()) { sendStatusInternal(client->id(), "error", "Missing or null 'value' field for set action."); return; }
                  JsonVariant newValueVariant = jsonDoc["value"];
                  if (setVariableValueInternal(varIndex, newValueVariant)) {
                      sendVariableValueInternal(client->id(), varIndex); 
                  } else {
                      sendStatusInternal(client->id(), "error", "Failed to set value (invalid type or out of limits).");
                  }
              }
          } 
          else if (strcmp(action, "start_stream") == 0) {
              if (_onStreamStartCallback != nullptr) {
                  if (!_isStreaming) {
                      Serial.println("Action: start_stream - Calling app callback.");
                      _onStreamStartCallback(); 
                      _isStreaming = true;      
                      sendStatusInternal(client->id(), "ok", "Stream started.");
                  } else { sendStatusInternal(client->id(), "info", "Stream was already active."); }
              } else {
                  Serial.println("Action: start_stream - No callback registered.");
                  sendStatusInternal(client->id(), "error", "Streaming feature not implemented/configured.");
              }
          } 
          else if (strcmp(action, "stop_stream") == 0) {
               if (_onStreamStopCallback != nullptr) {
                  if (_isStreaming) {
                      Serial.println("Action: stop_stream - Calling app callback.");
                      _onStreamStopCallback(); 
                      _isStreaming = false;     
                      sendStatusInternal(client->id(), "ok", "Stream stopped.");
                  } else { sendStatusInternal(client->id(), "info", "Stream was already stopped."); }
              } else {
                  Serial.println("Action: stop_stream - No callback registered.");
                  sendStatusInternal(client->id(), "error", "Streaming feature not implemented/configured.");
              }
          }
          else if (strcmp(action, "get_all_vars_config") == 0) {
              Serial.printf("Action: get_all_vars_config received from #%u\n", client->id());
              if (!_variables || _numVariables <= 0) {
                  sendStatusInternal(client->id(), "error", "No variables configured on server.");
                  return;
              }
              // Estimate JSON size: numVars * (avg name + type + value + limits + overhead)
              // Example: 10 vars * (20+10+10+20+5 + 30) = 10 * 95 = ~1KB
              const int avgBytesPerVar = 100; // Conservative estimate
              DynamicJsonDocument responseDoc(_numVariables * avgBytesPerVar + 128); // +128 for status and array wrapper

              responseDoc["status"] = "var_config_list";
              JsonArray varsArray = responseDoc.createNestedArray("variables");

              for (int i = 0; i < _numVariables; i++) {
                  JsonObject varObj = varsArray.createNestedObject();
                  varObj["name"] = _variables[i].name;
                  varObj["type"] = varTypeToCharString(_variables[i].type); // Use helper to convert enum to string
                  
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
              Serial.println("Sent var_config_list to client.");
          }
          else {
              Serial.printf("Unknown action received: %s\n", action);
              sendStatusInternal(client->id(), "error", "Unknown 'action' command.");
          }
        } 
        else if (info->opcode == WS_BINARY) {
             Serial.printf("Received Binary from #%u: %u bytes (ignored by library)\n", client->id(), len);
        }
      } 
      break;

    case WS_EVT_PONG: 
      // Serial.printf("WebSocket Pong received from #%u\n", client->id()); 
      break;

    case WS_EVT_ERROR:
      Serial.printf("WebSocket Client #%u error #%u: %s\n", client->id(), *((uint16_t*)arg), (char*)data);
      break;
  } 
}


// --- Public Function Implementations ---

/**
 * @brief Initializes WiFi AP, WebSocket server, and related components.
 */
void initWiFiWebSocketServer(const char *ssid, const char *password, 
                             VariableConfig *appVariables, int appNumVariables,
                             ArRequestHandlerFunction defaultRouteHandler) { 

  _variables = appVariables;
  _numVariables = appNumVariables;

  Serial.println("\n--- [LIB_CTRL] initWiFiWebSocketServer: START ---"); 

  if (!_variables || _numVariables <= 0) { 
      Serial.println("[LIB_CTRL] CRITICAL ERROR: Invalid variable array parameters received.");
      Serial.printf("[LIB_CTRL] appVariables pointer: %p, appNumVariables: %d\n", (void*)_variables, _numVariables);
      return; 
  }
  Serial.println("[LIB_CTRL] Variable array parameters check OK.");

  // Reset WiFi state for a cleaner start
  Serial.println("[LIB_CTRL] Attempting to reset WiFi state...");
  WiFi.persistent(false); // Don't save WiFi config to flash
  WiFi.disconnect(true);  // Disconnect and clear credentials if any
  WiFi.mode(WIFI_OFF);    // Turn off WiFi radio
  delay(100);             // Allow time for radio to turn off
  WiFi.mode(WIFI_AP);     // Set mode to Access Point
  Serial.println("[LIB_CTRL] WiFi state reset, AP mode set.");
  
  // Configure Static IP for the Access Point
  IPAddress apIP(192, 168, 5, 1);      // Desired static IP for the ESP32 AP
  IPAddress gatewayIP(192, 168, 5, 1); // Gateway is the ESP32 itself in AP mode
  IPAddress subnetMask(255, 255, 255, 0); // Standard subnet mask

  Serial.printf("[LIB_CTRL] Attempting to configure static AP IP: %s\n", apIP.toString().c_str());
  if (!WiFi.softAPConfig(apIP, gatewayIP, subnetMask)) {
    Serial.println("[LIB_CTRL] ERROR: Failed to configure static AP IP address!");
  } else {
      Serial.println("[LIB_CTRL] Static AP IP configuration successful.");
  }

  // Start the WiFi Access Point
  Serial.printf("[LIB_CTRL] Starting WiFi Access Point (SSID: %s)...\n", ssid);
  bool apStarted = WiFi.softAP(ssid, password); 

  if (apStarted) {
      Serial.println("[LIB_CTRL] Access Point started successfully.");
      IPAddress currentAPIP = WiFi.softAPIP();
      Serial.print("[LIB_CTRL] --> ESP32 Access Point IP Address: "); 
      Serial.println(currentAPIP); 
      if (currentAPIP != apIP && currentAPIP != IPAddress(0,0,0,0) && currentAPIP != IPAddress(192,168,4,1) /* Default if config failed */) {
          Serial.println("[LIB_CTRL] Warning: Actual AP IP does not match configured static IP. Check for conflicts or previous config errors.");
      }
  } else {
      Serial.println("[LIB_CTRL] CRITICAL ERROR: Failed to start Access Point!");
      return; 
  }

  // Configure WebSocket Server
  Serial.println("[LIB_CTRL] Configuring WebSocket server...");
  ws.onEvent(onWebSocketEvent); 
  server.addHandler(&ws);
  Serial.println("[LIB_CTRL] WebSocket handler attached to /ws endpoint."); 

  // Configure HTTP Server Root Route
  Serial.println("[LIB_CTRL] Configuring HTTP root route...");
  if (defaultRouteHandler) { 
      server.on("/", HTTP_GET, defaultRouteHandler); 
      Serial.println("[LIB_CTRL] Registered custom HTTP root route handler.");
  } else { 
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
          request->send(200, "text/plain", "ESP32 WebSocket Server Active. Connect to /ws"); 
      });
      Serial.println("[LIB_CTRL] Registered default HTTP root route handler.");
  }

  // Start the Web Server
  Serial.println("[LIB_CTRL] Starting HTTP server (server.begin())...");
  server.begin();
  Serial.println("[LIB_CTRL] HTTP & WebSocket Server started.");
  Serial.println("--- [LIB_CTRL] initWiFiWebSocketServer: COMPLETE ---"); 
}

/**
 * @brief Registers application callbacks for stream control commands.
 */
void setStreamCallbacks(StreamControlCallback onStart, StreamControlCallback onStop) {
    _onStreamStartCallback = onStart;
    _onStreamStopCallback = onStop;
    Serial.println("Stream control callbacks registered.");
}

/**
 * @brief Broadcasts a variable update (JSON) to all connected clients.
 */
void broadcastVariableUpdate(const char* variableName) {
    if (ws.count() == 0) return; 
    int index = findVariableIndexInternal(variableName);
    if (index < 0) {
        Serial.printf("Broadcast Error: Variable '%s' not found.\n", variableName);
        return;
    }
    StaticJsonDocument<256> jsonDoc; 
    VariableConfig& var = _variables[index];
    jsonDoc["variable"] = var.name;
    switch (var.type) {
        case TYPE_INT:    jsonDoc["value"] = var.intValue;    break;
        case TYPE_FLOAT:  jsonDoc["value"] = var.floatValue;  break;
        case TYPE_STRING: jsonDoc["value"] = var.stringValue; break;
        default: 
            Serial.printf("Broadcast Error: Unknown type for var '%s'\n", variableName);
            return; 
    }
    String response;
    serializeJson(jsonDoc, response);
    ws.textAll(response);
    // Serial.printf("Broadcast Sent: %s\n", response.c_str());
}

/**
 * @brief Broadcasts binary data to all connected clients.
 */
void broadcastBinaryData(const uint8_t* data, size_t len) {
    if (ws.count() > 0 && data != nullptr && len > 0) {
        ws.binaryAll(data, len);
    } 
}

/**
 * @brief Cleans up disconnected clients.
 */
void cleanupWebSocketClients() {
    ws.cleanupClients();
}