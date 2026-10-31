package com.vaibhavkishan.motopilot.protocol

import com.vaibhavkishan.motopilot.map.MapProjector
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class NavMessageMotionTest {
    private fun message(pxPerM: Double, ageMs: Int) = NavMessage(
        lat = 1.0, lon = 2.0, bearing = 90f, instruction = "Continue", distanceM = 10, street = "",
        routeScreenPoints = emptyList(), userScreenPoint = 204 to 204, speedMps = 8f,
        pxPerM = pxPerM, ageMs = ageMs
    )

    @Test
    fun sendsScaleAndFixAgeForMotionPrediction() {
        val json = JSONObject(message(0.58, 120).toJson())
        assertEquals(0.58, json.getDouble("px_per_m"), 1e-9)
        assertEquals(120, json.getInt("age_ms"))
    }

    @Test
    fun omitsTheScaleWhenUnknown() {
        assertFalse(JSONObject(message(0.0, 0).toJson()).has("px_per_m"))
    }

    @Test
    fun scaleMatchesTheProjection() {
        // 232 map pixels cover twice the map radius, so a 400 m radius is 0.29 px per metre.
        assertEquals(0.29, MapProjector.pixelsPerMeter(400.0), 1e-9)
        val one = MapProjector.projectUnclipped(0.0, 0.0, 0.0, 0.0, 400.0)
        val east100 = MapProjector.projectUnclipped(0.0, 0.0, 0.0, 100.0 / 111_320.0, 400.0)
        assertEquals(100 * MapProjector.pixelsPerMeter(400.0), east100.x - one.x, 1e-6)
        assertTrue(MapProjector.pixelsPerMeter(10.0) == MapProjector.pixelsPerMeter(50.0))
    }
}
