package com.vaibhavkishan.motopilot.routing

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RouteSplitTest {
    private val points = listOf(LatLon(0.0, 0.0), LatLon(0.0, 1.0), LatLon(0.0, 2.0), LatLon(0.0, 3.0))

    @Test
    fun splitsAtTheMatchedPosition() {
        val matched = LatLon(0.0, 1.5)
        val parts = RouteSplit.split(points, 1, matched)
        assertEquals(listOf(points[0], points[1], matched), parts.done)
        assertEquals(listOf(matched, points[2], points[3]), parts.ahead)
    }

    @Test
    fun withoutAMatchTheWholeRouteIsAhead() {
        val parts = RouteSplit.split(points, null, null)
        assertTrue(parts.done.isEmpty())
        assertEquals(points, parts.ahead)
    }

    @Test
    fun outOfRangeIndexKeepsTheWholeRouteAhead() {
        val parts = RouteSplit.split(points, 3, LatLon(0.0, 3.0))
        assertTrue(parts.done.isEmpty())
        assertEquals(points, parts.ahead)
    }
}
