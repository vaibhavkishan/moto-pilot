package com.vaibhavkishan.motopilot.protocol

import org.json.JSONArray
import org.json.JSONObject

/**
 * JSON navigation message sent to the ESP32.
 */
data class NavMessage(
    val lat: Double,
    val lon: Double,
    val bearing: Float,
    val instruction: String,
    val distanceM: Int,
    val street: String,
    val routeScreenPoints: List<Pair<Int, Int>>,
    /** Part of the route already travelled; the CYD draws it grey. */
    val routeDonePoints: List<Pair<Int, Int>> = emptyList(),
    /** Destination on the canvas, or null when it is outside the visible window. */
    val destScreenPoint: Pair<Int, Int>? = null,
    val streetSegments: List<IntArray> = emptyList(),
    val userScreenPoint: Pair<Int, Int>?,
    val html: String? = null,
    val remainingDistanceM: Int? = null,
    val remainingDurationS: Int? = null,
    val offRoute: Boolean = false,
    val gpsWeak: Boolean = false,
    val speedMps: Float = 0f,
    /** Canvas pixels per metre; lets the CYD predict movement between frames. 0 = unknown. */
    val pxPerM: Double = 0.0,
    /** How old the GPS fix already was when this frame was sent. */
    val ageMs: Int = 0,
    /** True only for a real GPS course while moving; the CYD holds its last heading otherwise. */
    val bearingValid: Boolean = false,
    /** Icon code for the next maneuver (see ManeuverIcon); empty when unknown. */
    val maneuver: String = "",
    /** Arrival clock time, HH:mm. */
    val eta: String? = null,
    val rerouting: Boolean = false,
    /** Wire name of the display style (see MapStyle); classic is omitted from the JSON. */
    val style: String = "classic",
    /** Share of the route already travelled, 0..1; null when unknown. */
    val tripProgress: Float? = null,
    /** Strongest zoom near turns (classic style); 1 switches the zoom off. */
    val zoomMax: Double = 3.5,
    /** Seconds before a turn at which the zoom starts. */
    val zoomLeadS: Int = 8
) {
    fun toJson(): String {
        val route = JSONArray()
        routeScreenPoints.forEach { (x, y) ->
            route.put(JSONArray().put(x).put(y))
        }
        val json = JSONObject()
            .put("type", "nav")
            .put("lat", lat)
            .put("lon", lon)
            .put("bearing", bearing.toDouble())
            .put("instruction", java.text.Normalizer.normalize(instruction, java.text.Normalizer.Form.NFC).take(63))
            .put("distance_m", distanceM)
            .put("remaining_m", remainingDistanceM ?: -1)
            .put("remaining_s", remainingDurationS ?: -1)
            .put("off_route", offRoute)
            .put("gps_weak", gpsWeak)
            .put("speed_mps", Math.round((if (speedMps.isFinite()) speedMps else 0f) * 10) / 10.0)
            .put("bearing_valid", bearingValid)
            .put("age_ms", ageMs.coerceIn(0, 5000))
            .put("rerouting", rerouting)
            .put("street", java.text.Normalizer.normalize(street, java.text.Normalizer.Form.NFC).take(47))
            .put("route", route)
        if (routeDonePoints.size >= 2) {
            val done = JSONArray()
            routeDonePoints.forEach { (x, y) -> done.put(JSONArray().put(x).put(y)) }
            json.put("route_done", done)
        }
        destScreenPoint?.let { (x, y) -> json.put("dest_x", x).put("dest_y", y) }
        if (streetSegments.isNotEmpty()) {
            val streets = JSONArray()
            streetSegments.forEach { seg ->
                if (seg.size >= 4) {
                    streets.put(JSONArray().put(seg[0]).put(seg[1]).put(seg[2]).put(seg[3]))
                }
            }
            if (streets.length() > 0) {
                json.put("streets", streets)
            }
        }
        userScreenPoint?.let { (x, y) ->
            json.put("user_x", x).put("user_y", y)
        }
        if (!html.isNullOrBlank()) {
            json.put("html", html.take(480))
        }
        if (maneuver.isNotEmpty()) json.put("maneuver", maneuver)
        if (pxPerM > 0.0) json.put("px_per_m", Math.round(pxPerM * 10000) / 10000.0)
        if (eta != null) json.put("eta", eta)
        if (style != "classic") json.put("style", style)
        if (style == "classic") {
            json.put("zoom_max", Math.round(zoomMax.coerceIn(1.0, 6.0) * 100) / 100.0)
            json.put("zoom_lead_s", zoomLeadS.coerceIn(1, 30))
        }
        tripProgress?.let { json.put("progress", Math.round(it.coerceIn(0f, 1f) * 1000) / 1000.0) }
        return json.toString()
    }

    companion object {
        fun heartbeat(): String = """{"type":"heartbeat"}"""

        fun loading(): String = """{"type":"loading"}"""

        fun arrived(): String = """{"type":"arrived"}"""

        /**
         * Light update sent between full frames: the newest fix and heading, no map geometry.
         * The CYD moves and turns the map with it.
         */
        fun motion(
            lat: Double,
            lon: Double,
            bearing: Float,
            bearingValid: Boolean,
            speedMps: Float,
            ageMs: Int,
            gpsWeak: Boolean
        ): String = JSONObject()
            .put("type", "motion")
            .put("lat", lat)
            .put("lon", lon)
            .put("bearing", Math.round(bearing * 10) / 10.0)
            .put("bearing_valid", bearingValid)
            .put("speed_mps", Math.round((if (speedMps.isFinite()) speedMps else 0f) * 10) / 10.0)
            .put("age_ms", ageMs.coerceIn(0, 5000))
            .put("gps_weak", gpsWeak)
            .toString()
    }
}
