package com.vaibhavkishan.motopilot.models

/** How the display draws the map. `wire` is the value of the `style` field sent to the CYD. */
enum class MapStyle(val storageKey: String, val wire: String) {
    CLASSIC("classic", "classic"),
    MINIMAL("minimal", "minimal");

    companion object {
        fun fromStorage(value: String?): MapStyle = entries.firstOrNull { it.storageKey == value } ?: CLASSIC
    }
}
