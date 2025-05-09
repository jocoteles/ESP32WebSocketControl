// js/main.js

/**
 * Main application script.
 * Initializes modules, sets up event listeners, and coordinates application logic.
 */

import appState from './appState.js';
import wsService from './websocketService.js';
import * as ui from './uiUpdater.js'; // Using namespace import for UI functions

// --- WebSocket Event Handlers (delegated from websocketService) ---

wsService.setOnOpen(() => {
    ui.updateConnectionStatus('Connected to ESP32', 'green');
    ui.loadVarsConfigBtnEl.disabled = false;
    sendLoadVarsConfigRequest(); // Automatically load config on successful connection
    ui.updateStreamControlUI(appState.isStreaming(), wsService.isConnected(), appState.getChunkCounter());
});

wsService.setOnClose((event) => {
    ui.updateConnectionStatus(`Disconnected (Code: ${event.code})`, 'red');
    appState.setStreaming(false); // Assume stream stops on disconnect
    ui.loadVarsConfigBtnEl.disabled = true;
    ui.updateStreamControlUI(appState.isStreaming(), false, appState.getChunkCounter());
});

wsService.setOnError((error) => {
    ui.updateConnectionStatus('WebSocket Connection Error', 'red');
    appState.setStreaming(false);
    ui.loadVarsConfigBtnEl.disabled = true;
    ui.updateStreamControlUI(appState.isStreaming(), false, appState.getChunkCounter());
});

wsService.setOnConfigList((varsArray) => {
    const currentValues = {};
    const fullConfig = {};
    varsArray.forEach(varConfig => {
        currentValues[varConfig.name] = varConfig.value;
        fullConfig[varConfig.name] = { 
            type: varConfig.type,
            hasLimits: varConfig.hasLimits,
            min: varConfig.min,
            max: varConfig.max
        };
    });
    appState.setAllConfigurableVariablesData(currentValues);
    appState.setAllVariablesConfiguration(fullConfig);
    
    ui.renderVariablesTable(
        appState.getAllConfigurableVariablesData(), 
        appState.getAllVariablesConfiguration(),
        sendGetVariableRequest, // Pass action handlers to the table renderer
        sendSetVariableRequest
    );
    ui.updateConnectionStatus('Variable configuration loaded.', 'green');
    setTimeout(() => {
        if (wsService.isConnected()) {
            ui.updateConnectionStatus('Connected to ESP32', 'green');
        }
    }, 2500);
});

wsService.setOnVariableUpdate((message) => {
    appState.setConfigurableVariableValue(message.variable, message.value);
    ui.renderVariablesTable(
        appState.getAllConfigurableVariablesData(), 
        appState.getAllVariablesConfiguration(),
        sendGetVariableRequest,
        sendSetVariableRequest
    );
});

wsService.setOnBinaryData((arrayBuffer) => {
    appState.incrementChunkCounter();
    ui.processAndLogBinaryData(arrayBuffer, appState.getChunkCounter());
    // Stream status text is updated inside updateStreamControlUI based on appState
    ui.updateStreamControlUI(appState.isStreaming(), wsService.isConnected(), appState.getChunkCounter());
});

wsService.setOnServerStatus((message) => {
    ui.handleServerStatusMessageForUI(message); // Let UI module decide how to display general status
    const msgText = message.message.toLowerCase();
    let streamingStateChanged = false;

    if (msgText.includes("stream iniciado") || msgText.includes("stream já estava ativo")) {
        if (!appState.isStreaming() || msgText.includes("stream iniciado")) { 
            appState.resetChunkCounter(); // Reset counter only if it *just* started or wasn't streaming
        }
        appState.setStreaming(true);
        streamingStateChanged = true;
    } else if (msgText.includes("stream parado") || msgText.includes("stream já estava parado")) {
        if (appState.isStreaming()) { 
             appState.setStreaming(false);
             streamingStateChanged = true;
        }
    }
    if (streamingStateChanged) {
        ui.updateStreamControlUI(appState.isStreaming(), wsService.isConnected(), appState.getChunkCounter());
    }
});


// --- Action Functions (that send commands via WebSocket) ---

/**
 * Sends a request to the ESP32 to load all variable configurations.
 * Updates the UI to indicate loading.
 */
function sendLoadVarsConfigRequest() {
    ui.renderVariablesTable({}, {}, sendGetVariableRequest, sendSetVariableRequest); // Clear table with placeholder
    const tableBody = document.getElementById('variablesTableBody');
    if(tableBody) tableBody.innerHTML = '<tr><td colspan="4" style="text-align: center; color: grey;">Loading variable configuration from ESP32...</td></tr>';
    wsService.sendPayload({ action: 'get_all_vars_config' });
}

/**
 * Sends a "get" request for a specific variable.
 * @param {string} variableName The name of the variable to get.
 */
function sendGetVariableRequest(variableName) { 
    wsService.sendPayload({ action: 'get', variable: variableName }); 
}

/**
 * Sends a "set" request for a specific variable.
 * Performs client-side validation based on the variable's configuration.
 * @param {string} variableName The name of the variable to set.
 * @param {string} rawValue The raw string value from the input field.
 * @param {object} varConfig The configuration object for this variable (from appState).
 */
function sendSetVariableRequest(variableName, rawValue, varConfig) {
    // Note: varConfig is passed from ui.renderVariablesTable's setRequestHandler call.
    // If called directly, ensure varConfig is fetched from appState.
    if (!varConfig) {
        varConfig = appState.getVariableConfiguration(variableName) || {};
    }
    const espVarType = varConfig.type;
    let valueToSend = rawValue;

    if (espVarType === "INT" || espVarType === "FLOAT") {
        const numericValue = Number(rawValue);
        if (isNaN(numericValue)) {
            alert(`Invalid value ('${rawValue}') for numeric variable '${variableName}'. Please enter a valid number.`); 
            return; 
        }
        if (varConfig.hasLimits) { 
            if (varConfig.min !== undefined && numericValue < varConfig.min) {
                alert(`Value ${numericValue} for '${variableName}' is less than the minimum allowed (${varConfig.min}).`); 
                return;
            }
            if (varConfig.max !== undefined && numericValue > varConfig.max) {
                alert(`Value ${numericValue} for '${variableName}' is greater than the maximum allowed (${varConfig.max}).`); 
                return;
            }
        }
        valueToSend = numericValue; 
    }
    wsService.sendPayload({ action: 'set', variable: variableName, value: valueToSend });
}

/**
 * Sends the "start_stream" command and updates UI optimistically.
 */
function sendStartStreamRequest() { 
    wsService.sendPayload({ action: 'start_stream' }); 
    appState.setStreaming(true); 
    appState.resetChunkCounter(); 
    if (ui.binaryDataLogAreaEl) ui.binaryDataLogAreaEl.textContent = "(Waiting for stream data...)\n"; 
    ui.updateStreamControlUI(appState.isStreaming(), wsService.isConnected(), appState.getChunkCounter());
}

/**
 * Sends the "stop_stream" command and updates UI optimistically.
 */
function sendStopStreamRequest() { 
    wsService.sendPayload({ action: 'stop_stream' }); 
    appState.setStreaming(false);
    ui.updateStreamControlUI(appState.isStreaming(), wsService.isConnected(), appState.getChunkCounter());
}

// --- Application Initialization ---

// This function runs once the DOM is fully loaded.
function initializeApp() {
    console.log('DOM Loaded. Main.js initializing application...');

    // Add event listeners to the main control buttons
    ui.loadVarsConfigBtnEl.addEventListener('click', sendLoadVarsConfigRequest);
    ui.startStreamBtnEl.addEventListener('click', sendStartStreamRequest);
    ui.stopStreamBtnEl.addEventListener('click', sendStopStreamRequest);
    
    // Set initial UI state for controls (mostly disabled until connected)
    ui.updateStreamControlUI(false, false, 0); 
    ui.loadVarsConfigBtnEl.disabled = true;

    // Attempt to connect to the WebSocket server
    wsService.connect(); 
}

// Start the application initialization when the DOM is ready.
document.addEventListener('DOMContentLoaded', initializeApp);