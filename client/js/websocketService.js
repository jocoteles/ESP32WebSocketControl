// js/websocketService.js

/**
 * Manages the WebSocket connection, message sending, and routing received messages.
 */

const ESP32_STATIC_IP = "192.168.5.1"; // Should match ESP32's static IP
const WEBSOCKET_URL = `ws://${ESP32_STATIC_IP}/ws`;

let ws; // The WebSocket instance

// Callback handlers to be set by other modules (e.g., main.js)
let onOpenHandler = () => {};
let onCloseHandler = () => {};
let onErrorHandler = () => {};
let onConfigListHandler = () => {};    // For the "var_config_list" message
let onVariableUpdateHandler = () => {}; // For individual variable updates
let onBinaryDataHandler = () => {};     // For binary stream data
let onServerStatusHandler = () => {};   // For general status messages

/**
 * Initializes and attempts to establish the WebSocket connection.
 */
function connect() {
    console.log(`WebSocket Service: Attempting to connect to ${WEBSOCKET_URL}`);
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
        console.log("WebSocket Service: Already connected or connecting.");
        return;
    }

    ws = new WebSocket(WEBSOCKET_URL);
    ws.binaryType = "arraybuffer"; // Expect binary data as ArrayBuffer

    ws.onopen = (event) => {
        console.log("WebSocket Service: Connection established.");
        onOpenHandler(event);
    };

    ws.onclose = (event) => {
        console.log(`WebSocket Service: Connection closed. Code: ${event.code}, Reason: ${event.reason || 'N/A'}`);
        ws = null; // Clear the instance
        onCloseHandler(event);
    };

    ws.onerror = (error) => {
        console.error("WebSocket Service: Error occurred.", error);
        // ws = null; // Could also clear here, but onclose usually follows
        onErrorHandler(error);
    };

    ws.onmessage = (event) => {
        if (event.data instanceof ArrayBuffer) {
            // console.log("WebSocket Service: Binary data received.");
            onBinaryDataHandler(event.data);
        } else if (typeof event.data === 'string') {
            // console.log("WebSocket Service: Text data received:", event.data);
            try {
                const message = JSON.parse(event.data);
                routeIncomingMessage(message);
            } catch (e) {
                console.error("WebSocket Service: Error parsing incoming JSON.", e, event.data);
            }
        } else {
            console.warn("WebSocket Service: Received unexpected data type.", typeof event.data);
        }
    };
}

/**
 * Routes parsed JSON messages to appropriate handlers.
 * @param {object} message The parsed JSON message from the server.
 */
function routeIncomingMessage(message) {
    if (message.status === "var_config_list" && message.variables) {
        onConfigListHandler(message.variables);
    } else if (message.status) { // General server status/error message
        onServerStatusHandler(message);
    } else if (message.variable !== undefined && message.value !== undefined) { // Single variable update
        onVariableUpdateHandler(message);
    } else {
        console.warn("WebSocket Service: Unhandled JSON message structure.", message);
    }
}

/**
 * Sends a JavaScript payload object as a JSON string via WebSocket.
 * @param {object} payload The JavaScript object to send.
 */
function sendPayload(payload) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        const jsonPayload = JSON.stringify(payload);
        console.log("WebSocket Service: Sending payload:", jsonPayload);
        ws.send(jsonPayload);
    } else {
        console.warn("WebSocket Service: Connection not open. Cannot send payload.", payload);
        // Optionally, trigger an error or UI update here
        // onErrorHandler({ message: "Connection not open to send data."}); 
    }
}

/**
 * Checks if the WebSocket connection is currently open.
 * @returns {boolean} True if connected, false otherwise.
 */
function isConnected() {
    return ws && ws.readyState === WebSocket.OPEN;
}

// Exported functions and setters for callbacks
export default {
    connect,
    sendPayload,
    isConnected,
    setOnOpen: (handler) => { onOpenHandler = handler; },
    setOnClose: (handler) => { onCloseHandler = handler; },
    setOnError: (handler) => { onErrorHandler = handler; },
    setOnConfigList: (handler) => { onConfigListHandler = handler; },
    setOnVariableUpdate: (handler) => { onVariableUpdateHandler = handler; },
    setOnBinaryData: (handler) => { onBinaryDataHandler = handler; },
    setOnServerStatus: (handler) => { onServerStatusHandler = handler; }
};