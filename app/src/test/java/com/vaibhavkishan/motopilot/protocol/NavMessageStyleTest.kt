package com.vaibhavkishan.motopilot.protocol

import com.vaibhavkishan.motopilot.models.MapStyle
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class NavMessageStyleTest {
    private fun message(style: MapStyle, progress: Float?) = NavMessage(
        lat = 1.0, lon = 2.0, bearing = 90f, instruction = "Turn right", distanceM = 300, street = "",
        routeScreenPoints = emptyList(), userScreenPoint = 204 to 204,
        style = style.wire, tripProgress = progress
    )

    @Test
    fun classicStyleIsOmittedAndMinimalIsSent() {
        assertFalse(JSONObject(message(MapStyle.CLASSIC, null).toJson()).has("style"))
        assertEquals("minimal", JSONObject(message(MapStyle.MINIMAL, null).toJson()).getString("style"))
    }

    @Test
    fun tripProgressIsClampedAndOptional() {
        assertFalse(JSONObject(message(MapStyle.MINIMAL, null).toJson()).has("progress"))
        assertEquals(0.4, JSONObject(message(MapStyle.MINIMAL, 0.4f).toJson()).getDouble("progress"), 1e-6)
        assertEquals(1.0, JSONObject(message(MapStyle.MINIMAL, 1.7f).toJson()).getDouble("progress"), 1e-6)
    }

    @Test
    fun unknownStoredStyleFallsBackToClassic() {
        assertEquals(MapStyle.CLASSIC, MapStyle.fromStorage(null))
        assertEquals(MapStyle.CLASSIC, MapStyle.fromStorage("neon"))
        assertEquals(MapStyle.MINIMAL, MapStyle.fromStorage("minimal"))
    }
}
