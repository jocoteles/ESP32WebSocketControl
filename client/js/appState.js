// js/appState.js

/**
 * Manages the shared client-side application state.
 */

// State variables
let configurableVariablesData = {}; // Stores current values of { varName: value }
let variablesConfiguration = {};    // Stores detailed config { varName: {type, min, max, hasLimits} }
let isClientStreaming = false;      // Tracks if the client believes the stream is active
let binaryChunkCounter = 0;         // Counts received binary data chunks

export default {
    // --- Get/Set for Configurable Variables ---
    setConfigurableVariableValue: (varName, value) => {
        configurableVariablesData[varName] = value;
    },
    getConfigurableVariableValue: (varName) => {
        return configurableVariablesData[varName];
    },
    getAllConfigurableVariablesData: () => {
        return configurableVariablesData;
    },
    setAllConfigurableVariablesData: (data) => {
        configurableVariablesData = data;
    },

    // --- Get/Set for Detailed Variable Configuration ---
    setVariableConfiguration: (varName, config) => {
        variablesConfiguration[varName] = config;
    },
    getVariableConfiguration: (varName) => {
        return variablesConfiguration[varName];
    },
    getAllVariablesConfiguration: () => {
        return variablesConfiguration;
    },
    setAllVariablesConfiguration: (config) => {
        variablesConfiguration = config;
    },

    // --- Stream State ---
    isStreaming: () => isClientStreaming,
    setStreaming: (status) => { 
        isClientStreaming = status; 
        if (!status) { // Reset counter if stopping
            // binaryChunkCounter = 0; // Or reset only on start
        }
    },

    // --- Binary Chunk Counter ---
    getChunkCounter: () => binaryChunkCounter,
    incrementChunkCounter: () => { binaryChunkCounter++; },
    resetChunkCounter: () => { binaryChunkCounter = 0; }
};