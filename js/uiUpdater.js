// js/uiUpdater.js
import appState from './appState.js'; // May need state for some UI updates

// --- DOM Element References (cached for performance) ---
const connectionStatusEl = document.getElementById('connectionStatus');
const variablesTableBodyEl = document.getElementById('variablesTableBody');
const startStreamBtnEl = document.getElementById('startStreamButton');
const stopStreamBtnEl = document.getElementById('stopStreamButton');
const streamStatusDisplayEl = document.getElementById('streamStatusDisplay');
const binaryDataLogAreaEl = document.getElementById('binaryDataLogArea');
const loadVarsConfigBtnEl = document.getElementById('loadVarsConfigButton');

/**
 * Updates the connection status display element.
 * @param {string} text The text to display.
 * @param {string} color The background color ('green', 'orange', 'red', or a CSS color).
 */
function updateConnectionStatus(text, color = 'grey') {
    if (!connectionStatusEl) return;
    connectionStatusEl.textContent = text;
    connectionStatusEl.style.backgroundColor = 
        color === 'green' ? '#d1e7dd' : 
        color === 'red' ? '#f8d7da' : 
        color === 'orange' ? '#fff3cd' : 
        '#e2e3e5'; 
    connectionStatusEl.style.color = 
        color === 'green' ? '#0f5132' : 
        color === 'red' ? '#842029' : 
        color === 'orange' ? '#664d03' : 
        '#41464b'; 
}

/**
 * Renders the table of configurable variables.
 * @param {object} currentValuesData - Object postaci { varName: value }
 * @param {object} configurationData - Object postaci { varName: {type, min, max, hasLimits} }
 * @param {function} getRequestHandler - Function to call when 'Get' is clicked (sends WS request).
 * @param {function} setRequestHandler - Function to call when 'Set' is clicked (sends WS request).
 */
function renderVariablesTable(currentValuesData, configurationData, getRequestHandler, setRequestHandler) {
    if (!variablesTableBodyEl) return;
    variablesTableBodyEl.innerHTML = ''; 
    const variableNames = Object.keys(currentValuesData).sort(); 

    if (variableNames.length === 0) {
         variablesTableBodyEl.innerHTML = '<tr><td colspan="4" style="text-align: center; color: grey;">No variables loaded. Click "Load Variables Configuration...".</td></tr>';
         return;
    }

    variableNames.forEach(varName => {
        const varValue = currentValuesData[varName];
        const varConfig = configurationData[varName] || {}; 
        const espVarType = varConfig.type || "UNKNOWN"; 
        
        const row = variablesTableBodyEl.insertRow(); 
        
        const nameCell = row.insertCell();
        nameCell.setAttribute('data-label', 'Variable: ');
        nameCell.textContent = varName; 
        
        const valueCell = row.insertCell(); 
        valueCell.setAttribute('data-label', 'Value: ');
        valueCell.textContent = (espVarType === 'STRING') ? `"${varValue}"` : varValue; 
        valueCell.style.wordBreak = 'break-all';
        
        const typeCell = row.insertCell();
        typeCell.setAttribute('data-label', 'Type: ');
        typeCell.textContent = espVarType; 

        const actionsCell = row.insertCell();
        actionsCell.setAttribute('data-label', 'Actions: ');
        actionsCell.classList.add('variable-actions'); 

        const input = document.createElement('input');
        if (espVarType === "INT" || espVarType === "FLOAT") {
            input.type = 'number';
            if (espVarType === "FLOAT") input.step = 'any'; 
            if (varConfig.hasLimits) { 
                if (varConfig.min !== undefined) input.min = varConfig.min;
                if (varConfig.max !== undefined) input.max = varConfig.max;
            }
        } else { 
            input.type = 'text';
        }
        input.id = `input-${varName}`; 
        input.placeholder = `New value`; 
        input.value = varValue; 

        const buttonWrapper = document.createElement('div');
        buttonWrapper.classList.add('action-buttons');

        const getButton = document.createElement('button');
        getButton.textContent = 'Get';
        getButton.classList.add('secondary', 'outline');
        getButton.onclick = () => getRequestHandler(varName); 

        const setButton = document.createElement('button');
        setButton.textContent = 'Set';
        setButton.onclick = () => { // Get value from input at click time
            const currentInputValue = document.getElementById(`input-${varName}`).value;
            setRequestHandler(varName, currentInputValue, varConfig); // Pass varConfig for validation
        };

        buttonWrapper.appendChild(getButton);
        buttonWrapper.appendChild(setButton);
        actionsCell.appendChild(input); 
        actionsCell.appendChild(buttonWrapper); 
    });
}

/**
 * Updates the UI elements related to stream control (buttons, status text).
 * @param {boolean} isStreaming - Current streaming state.
 * @param {boolean} isConnected - Current WebSocket connection state.
 * @param {number} chunkCounter - Current binary chunk counter.
 */
function updateStreamControlUI(isStreaming, isConnected, chunkCounter) {
    if (!streamStatusDisplayEl || !startStreamBtnEl || !stopStreamBtnEl) return;

    streamStatusDisplayEl.textContent = isStreaming 
        ? `Stream active. Chunks received: ${chunkCounter}` 
        : 'Stream stopped.';
    streamStatusDisplayEl.style.color = isStreaming ? 'var(--pico-color-green-600)' : 'var(--pico-muted-color)';
    
    startStreamBtnEl.disabled = isStreaming || !isConnected;
    stopStreamBtnEl.disabled = !isStreaming || !isConnected;
    
    startStreamBtnEl.setAttribute('aria-busy', isConnected && isStreaming ? 'true' : 'false'); 
    stopStreamBtnEl.setAttribute('aria-busy', isConnected && !isStreaming ? 'true' : 'false'); 

    if (!isStreaming && chunkCounter === 0 && isConnected) { 
        if (binaryDataLogAreaEl) binaryDataLogAreaEl.textContent = "(Stream data log will appear here)\n"; 
    }
}


/**
 * Appends text to the binary data log area, limiting its total size.
 * @param {string} text Text to append.
 */
function logToBinaryArea(text) {
    if (!binaryDataLogAreaEl) return;
    binaryDataLogAreaEl.textContent = text + binaryDataLogAreaEl.textContent.substring(0, 4000); 
}

/**
 * Processes received binary data (ArrayBuffer) for display.
 * @param {ArrayBuffer} arrayBuffer Raw binary data.
 * @param {number} currentChunkCounter The current global chunk counter.
 */
function processAndLogBinaryData(arrayBuffer, currentChunkCounter) {
    // Constants for Binary Data Processing (must match C++ struct)
    const SENSOR_PACKET_BYTES = 16; 
    const SENSORS_PER_PACKET = 6;   

    if (arrayBuffer.byteLength === 0 || arrayBuffer.byteLength % SENSOR_PACKET_BYTES !== 0) {
        console.error(`Binary Error: Buffer size (${arrayBuffer.byteLength}) invalid or not a multiple of ${SENSOR_PACKET_BYTES}.`);
        logToBinaryArea(`[Error: Invalid binary buffer received! Size: ${arrayBuffer.byteLength}]\n`);
        return;
    }
    const numPacketsInChunk = arrayBuffer.byteLength / SENSOR_PACKET_BYTES;
    const dataView = new DataView(arrayBuffer);
    let chunkLogContent = ""; 

    for (let i = 0; i < numPacketsInChunk; i++) {
        const offset = i * SENSOR_PACKET_BYTES; 
        const sensorReadings = []; 
        for(let s = 0; s < SENSORS_PER_PACKET; s++) {
            sensorReadings.push(dataView.getUint16(offset + (s * 2), true)); 
        }
        const timeMs = dataView.getUint32(offset + (SENSORS_PER_PACKET * 2), true); 

        if (i === 0 || i === numPacketsInChunk - 1) { 
           chunkLogContent += ` C${currentChunkCounter} P${i}: [${sensorReadings.join(', ')}] @ ${timeMs}ms\n`;
        } else if (i === 1 && numPacketsInChunk > 2) { 
            chunkLogContent += ` C${currentChunkCounter} P${i}: ... (${numPacketsInChunk - 2} samples omitted) ...\n`;
        }
        // TODO: Add actual data processing here (e.g., plotting to a chart)
    }
    logToBinaryArea(chunkLogContent); 
}

/**
 * Handles server status messages for UI feedback.
 * @param {object} message The status message object from the server.
 */
function handleServerStatusMessageForUI(message) {
    console.log(`UI Handler: ESP32 Status [${message.status}]: ${message.message}`);
    // Example: Display a temporary notification, or update a specific status area.
    // For now, we primarily rely on updateConnectionStatus and updateStreamControlUI
    // which are called based on state changes triggered by these messages in main.js.
}


// Export UI update functions to be used by main.js
export {
    updateConnectionStatus,
    renderVariablesTable,
    updateStreamControlUI,
    logToBinaryArea,
    processAndLogBinaryData,
    handleServerStatusMessageForUI,
    loadVarsConfigBtnEl, // Exporting for main.js to enable/disable
    startStreamBtnEl,    // Exporting for main.js to enable/disable
    stopStreamBtnEl      // Exporting for main.js to enable/disable
};