package com.vaibhavkishan.motopilot.models

/** What the Device Connection screen shows; one phase at a time, never contradictory. */
enum class ConnectionPhase { NOT_CONNECTED, SCANNING, CONNECTING, RECONNECTING, CONNECTED, FAILED }

fun connectionPhase(state: ConnectionState, isScanning: Boolean): ConnectionPhase = when (state) {
    ConnectionState.CONNECTED -> ConnectionPhase.CONNECTED
    ConnectionState.CONNECTING -> ConnectionPhase.CONNECTING
    ConnectionState.RECONNECTING -> ConnectionPhase.RECONNECTING
    ConnectionState.ERROR -> if (isScanning) ConnectionPhase.SCANNING else ConnectionPhase.FAILED
    ConnectionState.DISCONNECTED -> if (isScanning) ConnectionPhase.SCANNING else ConnectionPhase.NOT_CONNECTED
}

enum class SignalLevel { STRONG, FAIR, WEAK }

fun signalLevel(rssi: Int): SignalLevel = when {
    rssi >= -60 -> SignalLevel.STRONG
    rssi >= -75 -> SignalLevel.FAIR
    else -> SignalLevel.WEAK
}
