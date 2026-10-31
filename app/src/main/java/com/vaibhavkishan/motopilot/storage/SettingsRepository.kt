package com.vaibhavkishan.motopilot.storage

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.doublePreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import com.vaibhavkishan.motopilot.models.AppSettings
import com.vaibhavkishan.motopilot.models.MapStyle
import com.vaibhavkishan.motopilot.routing.RouteProfile
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "display_connect_settings")

class SettingsRepository(private val context: Context) {

    val settings: Flow<AppSettings> = context.dataStore.data.map { prefs -> readSettings(prefs) }

    suspend fun updateBleDevice(address: String, name: String) {
        context.dataStore.edit { prefs ->
            prefs[Keys.BLE_DEVICE_ADDRESS] = address
            prefs[Keys.BLE_DEVICE_NAME] = name
        }
    }

    suspend fun updateSettings(transform: (AppSettings) -> AppSettings) {
        context.dataStore.edit { prefs ->
            val updated = transform(readSettings(prefs))
            prefs[Keys.BLE_DEVICE_ADDRESS] = updated.bleDeviceAddress
            prefs[Keys.BLE_DEVICE_NAME] = updated.bleDeviceName
            prefs[Keys.NAV_UPDATE_HZ] = updated.navUpdateHz
            prefs[Keys.MAP_SCALE_METERS] = updated.mapScaleMeters
            prefs[Keys.ROUTE_PROFILE] = updated.routeProfile.storageKey
            prefs[Keys.MAP_STYLE] = updated.mapStyle.storageKey
            prefs[Keys.ZOOM_ENABLED] = updated.zoomEnabled
            prefs[Keys.ZOOM_MAX] = updated.zoomMax
            prefs[Keys.ZOOM_LEAD_SECONDS] = updated.zoomLeadSeconds
            prefs[Keys.DEST_QUERY] = updated.destQuery
            prefs[Keys.DEST_LABEL] = updated.destLabel
            prefs[Keys.DEST_LAT] = updated.destLat
            prefs[Keys.DEST_LON] = updated.destLon
        }
    }

    private fun readSettings(prefs: Preferences): AppSettings {
        val defaults = AppSettings()
        return AppSettings(
            bleDeviceAddress = prefs[Keys.BLE_DEVICE_ADDRESS] ?: "",
            bleDeviceName = prefs[Keys.BLE_DEVICE_NAME] ?: "",
            navUpdateHz = prefs[Keys.NAV_UPDATE_HZ] ?: defaults.navUpdateHz,
            mapScaleMeters = prefs[Keys.MAP_SCALE_METERS] ?: defaults.mapScaleMeters,
            routeProfile = RouteProfile.fromStorage(prefs[Keys.ROUTE_PROFILE]),
            mapStyle = MapStyle.fromStorage(prefs[Keys.MAP_STYLE]),
            zoomEnabled = prefs[Keys.ZOOM_ENABLED] ?: defaults.zoomEnabled,
            zoomMax = prefs[Keys.ZOOM_MAX] ?: defaults.zoomMax,
            zoomLeadSeconds = prefs[Keys.ZOOM_LEAD_SECONDS] ?: defaults.zoomLeadSeconds,
            destQuery = prefs[Keys.DEST_QUERY] ?: "",
            destLabel = prefs[Keys.DEST_LABEL] ?: "",
            destLat = prefs[Keys.DEST_LAT] ?: "",
            destLon = prefs[Keys.DEST_LON] ?: ""
        )
    }

    private object Keys {
        val BLE_DEVICE_ADDRESS = stringPreferencesKey("ble_device_address")
        val BLE_DEVICE_NAME = stringPreferencesKey("ble_device_name")
        val NAV_UPDATE_HZ = intPreferencesKey("nav_update_hz")
        val MAP_SCALE_METERS = doublePreferencesKey("map_scale_meters")
        val ROUTE_PROFILE = stringPreferencesKey("route_profile")
        val MAP_STYLE = stringPreferencesKey("map_style")
        val ZOOM_ENABLED = booleanPreferencesKey("zoom_enabled")
        val ZOOM_MAX = doublePreferencesKey("zoom_max")
        val ZOOM_LEAD_SECONDS = intPreferencesKey("zoom_lead_seconds")
        val DEST_QUERY = stringPreferencesKey("dest_query")
        val DEST_LABEL = stringPreferencesKey("dest_label")
        val DEST_LAT = stringPreferencesKey("dest_lat")
        val DEST_LON = stringPreferencesKey("dest_lon")
    }
}
