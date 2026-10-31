package com.vaibhavkishan.motopilot.protocol

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class NavMessageLightFrameTest {
    private fun message(style: String, zoomMax: Double = 3.5, zoomLeadS: Int = 8) = NavMessage(
        lat = 1.0, lon = 2.0, bearing = 10f, instruction = "", distanceM = 0, street = "",
        routeScreenPoints = emptyList(), userScreenPoint = null,
        style = style, zoomMax = zoomMax, zoomLeadS = zoomLeadS
    )

    @Test
    fun motionUpdateCarriesOnlyTheFixAndHeading() {
        val json = JSONObject(NavMessage.motion(48.1234567, 11.7654321, 91.26f, true, 12.34f, 180, false))
        assertEquals("motion", json.getString("type"))
        assertEquals(48.1234567, json.getDouble("lat"), 1e-9)
        assertEquals(11.7654321, json.getDouble("lon"), 1e-9)
        assertEquals(91.3, json.getDouble("bearing"), 1e-6)
        assertTrue(json.getBoolean("bearing_valid"))
        assertEquals(12.3, json.getDouble("speed_mps"), 1e-6)
        assertEquals(180, json.getInt("age_ms"))
        assertFalse(json.getBoolean("gps_weak"))
        assertFalse(json.has("route"))
        assertTrue(NavMessage.motion(0.0, 0.0, 0f, false, 0f, 0, false).length < 160)
    }

    @Test
    fun motionUpdateClampsAgeAndSpeed() {
        val json = JSONObject(NavMessage.motion(0.0, 0.0, 0f, false, Float.NaN, 99_999, true))
        assertEquals(5000, json.getInt("age_ms"))
        assertEquals(0.0, json.getDouble("speed_mps"), 1e-9)
        assertTrue(json.getBoolean("gps_weak"))
    }

    @Test
    fun classicFramesCarryTheZoomSettings() {
        val json = JSONObject(message("classic", zoomMax = 2.5, zoomLeadS = 11).toJson())
        assertEquals(2.5, json.getDouble("zoom_max"), 1e-9)
        assertEquals(11, json.getInt("zoom_lead_s"))
    }

    @Test
    fun zoomOffIsSentAsNoMagnificationAndSettingsAreClamped() {
        assertEquals(1.0, JSONObject(message("classic", zoomMax = 1.0).toJson()).getDouble("zoom_max"), 1e-9)
        assertEquals(6.0, JSONObject(message("classic", zoomMax = 40.0).toJson()).getDouble("zoom_max"), 1e-9)
        assertEquals(30, JSONObject(message("classic", zoomLeadS = 500).toJson()).getInt("zoom_lead_s"))
    }

    @Test
    fun minimalFramesHaveNoZoomFields() {
        val json = JSONObject(message("minimal").toJson())
        assertFalse(json.has("zoom_max"))
        assertFalse(json.has("zoom_lead_s"))
    }
}
