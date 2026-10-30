package com.vaibhavkishan.motopilot.models

import com.vaibhavkishan.motopilot.network.BleDeviceItem
import com.vaibhavkishan.motopilot.routing.PlaceSearchResult
import com.vaibhavkishan.motopilot.routing.RouteProfile

data class MainUiState(
    val bleDeviceAddress: String = "",
    val bleDeviceName: String = "",
    val scannedDevices: List<BleDeviceItem> = emptyList(),
    val isScanning: Boolean = false,
    val destQuery: String = "",
    val searchCity: String = "",
    val destLabel: String = "",
    val destLat: String = "",
    val destLon: String = "",
    val routeProfile: RouteProfile = RouteProfile.CAR,
    val searchResults: List<PlaceSearchResult> = emptyList(),
    val isSearching: Boolean = false,
    val connectionState: ConnectionState = ConnectionState.DISCONNECTED,
    val isNavigating: Boolean = false,
    /** Start was requested but navigation is not running yet (permission prompt or GPS fix pending). */
    val isStarting: Boolean = false,
    val stats: TransmissionStats = TransmissionStats(),
    val errorMessage: String? = null
)
