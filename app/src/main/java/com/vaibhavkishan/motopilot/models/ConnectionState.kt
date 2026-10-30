package com.vaibhavkishan.motopilot.models

/**
 * Connection state with the ESP32.
 */
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    ERROR
}
