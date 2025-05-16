/**
 * @file main.cpp
 * @brief Example application demonstrating the use of ESP32WebSocket library.
 *        Implements both Get/Set variable control via JSON and high-frequency 
 *        binary data streaming of 6 analog inputs.
 */

// Include our custom WebSocket communication library
#include "ESP32WebSocket.h" 

// --- WiFi Access Point Configuration ---
const char *WIFI_SSID = "ESP32_Control_AP";      // Network name for clients to connect to
const char *WIFI_PASSWORD = "password123"; // Network password (min 8 chars)
const uint8_t DESIRED_STATIC_IP[4] = {192, 168, 5, 1}; // Desired IP

// --- Get/Set Variable Configuration (JSON Communication) ---

// Define the variables that can be read/written via JSON commands
// (Uses the VariableConfig struct from ESP32WebSocket.h)
VariableConfig configurableVariables[] = {
  // Name             Type        Int Val  Float Val  String Val   Min     Max     Limits?
  {"led_intensity",   TYPE_INT,   128,     0.0f,      "",          0.0,    255.0,  true},
  {"update_interval", TYPE_INT,   500,     0.0f,      "",          50.0,   5000.0, true},
  {"motor_enable",    TYPE_INT,   0,       0.0f,      "",          0.0,    1.0,    true}, // Example: 0=off, 1=on
  {"device_label",    TYPE_STRING,0,       0.0f,      "ESP32-01",  0.0,    0.0,    false} 
};
// Automatically calculate the number of configurable variables
const int numConfigurableVariables = sizeof(configurableVariables) / sizeof(configurableVariables[0]);


// --- Real Time Reading (Streaming) Configuration ---

// Constants for data acquisition and buffering
const int SAMPLES_PER_CHUNK = 25;       // How many readings to buffer before sending (N)
const int SAMPLE_INTERVAL_US = 250;     // Time between samples in microseconds (adjust for desired rate)

// Define the 6 Analog Input pins to be read
// Note: Ensure these pins are suitable for ADC use on your specific ESP32 board.
//       ADC1 pins (GPIO 32-39) are generally safe. Avoid ADC2 pins if using WiFi.
const int ANALOG_PIN_1 = 32; 
const int ANALOG_PIN_2 = 33;
const int ANALOG_PIN_3 = 34;
const int ANALOG_PIN_4 = 35;
const int ANALOG_PIN_5 = 36; // Often VP
const int ANALOG_PIN_6 = 39; // Often VN

// Structure for a single packet of sensor data (matches client-side expectation)
// Use pragma pack to ensure no padding bytes are added by the compiler.
#pragma pack(push, 1) 
struct SensorDataPacket {
  uint16_t reading1;    // Reading from ANALOG_PIN_1
  uint16_t reading2;    // Reading from ANALOG_PIN_2
  uint16_t reading3;    // Reading from ANALOG_PIN_3
  uint16_t reading4;    // Reading from ANALOG_PIN_4
  uint16_t reading5;    // Reading from ANALOG_PIN_5
  uint16_t reading6;    // Reading from ANALOG_PIN_6
  uint32_t time_ms;     // Timestamp (milliseconds) relative to stream start
};                      // Total size = 6 * 2 + 4 = 16 bytes
#pragma pack(pop) 

// Calculate sizes based on the structure and chunk configuration
const int PACKET_SIZE_BYTES = sizeof(SensorDataPacket);       // Size of one packet (16 bytes)
const int CHUNK_BUFFER_SIZE_BYTES = SAMPLES_PER_CHUNK * PACKET_SIZE_BYTES; // Total size of the send buffer

// Buffer to hold sensor data before sending
SensorDataPacket sensorDataBuffer[SAMPLES_PER_CHUNK];
// Index pointing to the next available slot in the buffer
int currentBufferIndex = 0;

// --- Application State Variables ---
bool isAppStreaming = false;      // Flag controlled by callbacks, enables/disables streaming logic in loop()
uint32_t streamStartTimeMs = 0;   // Records the time when streaming started (using millis())


// --- Stream Control Callback Functions (Required by the Library) ---

/**
 * @brief This function is called by the ESP32WebSocketControl library 
 *        when a "start_stream" command is received from a client.
 *        It sets up the application state to begin data acquisition.
 */
void application_onStreamStart() {
  Serial.println("Application Callback: START STREAM requested.");
  currentBufferIndex = 0;          // Reset buffer index
  streamStartTimeMs = millis();    // Record the start time for relative timestamps
  isAppStreaming = true;           // Set the flag to enable streaming logic in loop()
  // Optional: Could add actions like enabling sensor power here.
}

/**
 * @brief This function is called by the ESP32WebSocketControl library
 *        when a "stop_stream" command is received or the last client disconnects.
 *        It sets the application state to stop data acquisition.
 */
void application_onStreamStop() {
  Serial.println("Application Callback: STOP STREAM requested.");
  isAppStreaming = false;          // Clear the flag to disable streaming logic in loop()
  // Optional: Could add actions like disabling sensor power here.
}


// --- Arduino Setup Function ---

/**
 * @brief Initializes Serial communication, configures pins, and starts the 
 *        WiFi/WebSocket server using the ESP32WebSocketControl library.
 */
void setup() {
  // Start Serial communication for debugging and status messages
  Serial.begin(115200);
  delay(500); // Pequena pausa para garantir que o Serial esteja pronto
  Serial.println("\n\n--- [APP_DEMO] Setup: START ---"); // << NOVO LOG

  // Configure the specified analog input pins
  Serial.println("[APP_DEMO] Setup: Configuring analog pins..."); // << NOVO LOG
  pinMode(ANALOG_PIN_1, INPUT); 
  pinMode(ANALOG_PIN_2, INPUT); 
  pinMode(ANALOG_PIN_3, INPUT);
  pinMode(ANALOG_PIN_4, INPUT); 
  pinMode(ANALOG_PIN_5, INPUT); 
  pinMode(ANALOG_PIN_6, INPUT);
  Serial.println("[APP_DEMO] Setup: Analog pins configured."); // << NOVO LOG
  
  Serial.println("[APP_DEMO] Setup: Calling initWiFiWebSocketServer..."); // << NOVO LOG
  // Call the init function
  initWiFiWebSocketServer(
    WIFI_SSID,
    WIFI_PASSWORD,
    DESIRED_STATIC_IP,
    configurableVariables,
    numConfigurableVariables
  ); 
  Serial.println("[APP_DEMO] Setup: initWiFiWebSocketServer CALL RETURNED."); // << NOVO LOG

  Serial.println("[APP_DEMO] Setup: Calling setStreamCallbacks..."); // << NOVO LOG
  setStreamCallbacks(application_onStreamStart, application_onStreamStop);
  Serial.println("[APP_DEMO] Setup: setStreamCallbacks CALL RETURNED."); // << NOVO LOG

  Serial.printf("[APP_DEMO] Streaming Config: %d samples/chunk, %d us/sample interval.\n", 
                SAMPLES_PER_CHUNK, SAMPLE_INTERVAL_US);
  Serial.printf("[APP_DEMO] Data Packet Size: %d bytes. Chunk Buffer Size: %d bytes.\n", 
                PACKET_SIZE_BYTES, CHUNK_BUFFER_SIZE_BYTES);

  Serial.println("--- [APP_DEMO] Setup: COMPLETE ---"); // << NOVO LOG
  Serial.println("[APP_DEMO] Waiting for client connections...");
}


// --- Arduino Loop Function ---

/**
 * @brief Main application loop. Handles the real-time data acquisition and buffering
 *        when streaming is active. The WebSocket communication itself is handled 
 *        asynchronously by the ESP32WebSocketControl library in the background.
 */
void loop() {

  // Check if the application should be actively streaming data
  if (isAppStreaming) {
    
    // --- 1. Read Analog Sensors ---
    // Read the voltage level from each configured analog pin.
    // The raw reading is typically 0-4095 for 12-bit ADC.
    uint16_t val1 = analogRead(ANALOG_PIN_1);
    uint16_t val2 = analogRead(ANALOG_PIN_2);
    uint16_t val3 = analogRead(ANALOG_PIN_3);
    uint16_t val4 = analogRead(ANALOG_PIN_4);
    uint16_t val5 = analogRead(ANALOG_PIN_5);
    uint16_t val6 = analogRead(ANALOG_PIN_6);

    // --- 2. Get Timestamp ---
    // Calculate the time elapsed since the stream started.
    uint32_t currentTimeMs = millis() - streamStartTimeMs; 

    // --- 3. Fill Buffer ---
    // Store the readings and timestamp into the current slot of the data buffer.
    sensorDataBuffer[currentBufferIndex].reading1 = val1;
    sensorDataBuffer[currentBufferIndex].reading2 = val2;
    sensorDataBuffer[currentBufferIndex].reading3 = val3;
    sensorDataBuffer[currentBufferIndex].reading4 = val4;
    sensorDataBuffer[currentBufferIndex].reading5 = val5;
    sensorDataBuffer[currentBufferIndex].reading6 = val6;
    sensorDataBuffer[currentBufferIndex].time_ms = currentTimeMs;

    // Move to the next position in the buffer.
    currentBufferIndex++; 

    // --- 4. Check if Buffer is Full and Send ---
    // If the buffer has reached the configured chunk size...
    if (currentBufferIndex >= SAMPLES_PER_CHUNK) {
      // ...send the entire buffer as a single binary message to all connected clients
      // using the library function.
      broadcastBinaryData((uint8_t*)sensorDataBuffer, CHUNK_BUFFER_SIZE_BYTES);
      
      // Reset the buffer index to start filling the next chunk.
      currentBufferIndex = 0; 
    }

    // --- 5. Wait for Sample Interval ---
    // Pause execution for the configured time before taking the next sample.
    // Using delayMicroseconds allows for higher sampling rates than delay().
    delayMicroseconds(SAMPLE_INTERVAL_US); 

  } else {
    // --- Idle State (Not Streaming) ---
    // If streaming is not active, the application can perform other tasks
    // or simply wait to conserve power/CPU.
    
    // Example: Read a configurable variable and print it periodically
    static unsigned long lastPrintTime = 0;
    unsigned long interval = configurableVariables[1].intValue; // Use 'update_interval'
    if (millis() - lastPrintTime > interval) {
       lastPrintTime = millis();
       // Serial.printf("Idle: LED Intensity = %d\n", configurableVariables[0].intValue);
    }

    // A short delay when idle prevents the loop from running at maximum speed unnecessarily.
    delay(50); 
  }

  // Optional: Periodically clean up disconnected WebSocket clients.
  // Often not needed in the loop as the library handles much of it.
  // cleanupWebSocketClients();

} // End main loop()