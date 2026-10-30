package com.vaibhavkishan.motopilot.utils

import com.vaibhavkishan.motopilot.models.TransmissionStats
import com.vaibhavkishan.motopilot.routing.TripProgress
import com.vaibhavkishan.motopilot.offline.OfflineMapState
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow

object TransmissionHub {
    private val _gps = MutableStateFlow(com.vaibhavkishan.motopilot.navigation.GpsStatus())
    val gps = _gps.asStateFlow()
    fun updateGps(state: com.vaibhavkishan.motopilot.navigation.GpsStatus) { _gps.value = state }
    private val _offlineMap = MutableStateFlow(OfflineMapState())
    val offlineMap: StateFlow<OfflineMapState> = _offlineMap.asStateFlow()
    fun updateOfflineMap(state: OfflineMapState) {
        _offlineMap.update { state.copy(outsideCoverage = it.outsideCoverage) }
    }
    fun updateMapCoverage(complete: Boolean) { _offlineMap.update { it.copy(outsideCoverage = !complete) } }
    private val _navigationError = MutableStateFlow<String?>(null)
    val navigationError: StateFlow<String?> = _navigationError.asStateFlow()
    fun navigationFailed(message: String) { _navigationError.value = message }
    fun clearNavigationError() { _navigationError.value = null }
    private val _arrivals = MutableSharedFlow<Unit>(extraBufferCapacity = 1)
    val arrivals: SharedFlow<Unit> = _arrivals.asSharedFlow()
    fun announceArrival() { _arrivals.tryEmit(Unit) }
    private val _tripProgress = MutableStateFlow(TripProgress())
    val tripProgress: StateFlow<TripProgress> = _tripProgress.asStateFlow()

    fun updateTripProgress(progress: TripProgress) { _tripProgress.value = progress }

    private val _isNavigating = MutableStateFlow(false)
    val isNavigating: StateFlow<Boolean> = _isNavigating.asStateFlow()

    private val _stats = MutableStateFlow(TransmissionStats())
    val stats: StateFlow<TransmissionStats> = _stats.asStateFlow()

    fun setNavigating(active: Boolean) {
        _offlineMap.value = OfflineMapState()
        _gps.value = com.vaibhavkishan.motopilot.navigation.GpsStatus()
        _tripProgress.value = TripProgress()
        _isNavigating.value = active
        if (!active) {
            _stats.value = TransmissionStats()
        }
    }

    fun updateStats(stats: TransmissionStats) {
        _stats.value = stats
    }
}
