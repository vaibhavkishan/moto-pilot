package com.vaibhavkishan.motopilot.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.vaibhavkishan.motopilot.MotoPilotApplication
import com.vaibhavkishan.motopilot.models.AppSettings
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/**
 * ViewModel for the settings screen.
 */
class SettingsViewModel(application: Application) : AndroidViewModel(application) {
    val stats = com.vaibhavkishan.motopilot.utils.TransmissionHub.stats

    private val settingsRepository = (application as MotoPilotApplication).settingsRepository

    private val _settings = MutableStateFlow(AppSettings())
    val settings: StateFlow<AppSettings> = _settings.asStateFlow()

    init {
        viewModelScope.launch {
            settingsRepository.settings.collect { _settings.value = it }
        }
    }

    fun updateSettings(transform: (AppSettings) -> AppSettings) {
        viewModelScope.launch {
            settingsRepository.updateSettings(transform)
        }
    }
}
