package com.vaibhavkishan.motopilot.models

enum class CoordinateError { NOT_A_NUMBER, OUT_OF_RANGE, MISSING }

/** Destination coordinates typed by the user. Both values must be valid before navigation can start. */
object Coordinates {
    /** Accepts a decimal comma as well as a point. */
    fun parse(text: String): Double? =
        text.trim().replace(',', '.').toDoubleOrNull()?.takeIf { it.isFinite() }

    fun latitudeError(lat: String, lon: String): CoordinateError? = errorFor(lat, lon, 90.0)

    fun longitudeError(lat: String, lon: String): CoordinateError? = errorFor(lon, lat, 180.0)

    private fun errorFor(own: String, other: String, limit: Double): CoordinateError? {
        if (own.isBlank()) return if (other.isNotBlank()) CoordinateError.MISSING else null
        val value = parse(own) ?: return CoordinateError.NOT_A_NUMBER
        return if (value in -limit..limit) null else CoordinateError.OUT_OF_RANGE
    }

    fun resolve(lat: String, lon: String): Pair<Double, Double>? {
        val latitude = parse(lat) ?: return null
        val longitude = parse(lon) ?: return null
        if (latitude !in -90.0..90.0 || longitude !in -180.0..180.0) return null
        return latitude to longitude
    }
}

enum class StartBlocker { NONE, STARTING, NOT_CONNECTED, NO_DESTINATION, SEARCH_NOT_RUN, INVALID_COORDINATES }

/** Why "Start navigation" is unavailable, in the order the user should fix things. */
fun startBlocker(connected: Boolean, starting: Boolean, query: String, lat: String, lon: String): StartBlocker = when {
    starting -> StartBlocker.STARTING
    !connected -> StartBlocker.NOT_CONNECTED
    Coordinates.resolve(lat, lon) != null -> StartBlocker.NONE
    lat.isNotBlank() || lon.isNotBlank() -> StartBlocker.INVALID_COORDINATES
    query.isNotBlank() -> StartBlocker.SEARCH_NOT_RUN
    else -> StartBlocker.NO_DESTINATION
}
