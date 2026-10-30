package com.vaibhavkishan.motopilot.map

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.hypot
import kotlin.math.max

class MapProjectorTest {
    private val center = MapProjector.CANVAS_CENTER.toInt()

    @Test fun userIsAtTheCanvasCenter() {
        assertEquals(center to center, MapProjector.projectPoint(0.0, 0.0, 0.0, 0.0, 200.0))
    }

    @Test fun equalDistancesHaveEqualScaleOnBothAxes() {
        val delta = 100.0 / 111_320.0
        val east = MapProjector.projectPoint(0.0, 0.0, 0.0, delta, 200.0)
        val north = MapProjector.projectPoint(0.0, 0.0, delta, 0.0, 200.0)
        assertEquals(58, east.first - center)
        assertEquals(east.first - center, center - north.second)
    }

    @Test fun farCoordinatesStayWithinCanvas() {
        val last = MapProjector.CANVAS_SIZE - 1
        assertEquals(last to 0, MapProjector.projectPoint(0.0, 0.0, 1.0, 1.0, 200.0))
        assertEquals(0 to last, MapProjector.projectPoint(0.0, 0.0, -1.0, -1.0, 200.0))
    }

    @Test fun windowCoversTheVisibleAreaAtAnyRotation() {
        val farthestCorner = hypot(
            max(MapProjector.ANCHOR_X, MapProjector.MAP_WIDTH - MapProjector.ANCHOR_X).toDouble(),
            max(MapProjector.ANCHOR_Y, MapProjector.MAP_HEIGHT - MapProjector.ANCHOR_Y).toDouble()
        )
        assertTrue(MapProjector.CANVAS_RADIUS >= farthestCorner)
        assertTrue(MapProjector.CANVAS_CENTER + MapProjector.CANVAS_RADIUS <= MapProjector.CANVAS_SIZE)
    }
}
