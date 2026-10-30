package com.vaibhavkishan.motopilot.map

import com.vaibhavkishan.motopilot.routing.LatLon
import org.junit.Assert.*
import org.junit.Test

class StreetGeometryTest {
    private val c = MapProjector.CANVAS_CENTER

    /** Canvas pixel to geographic point; at scale 116 one pixel is one metre. */
    private fun geo(x: Double, y: Double) = LatLon((c - y) / 111_320.0, (x - c) / 111_320.0)
    private fun segments(ways: List<List<LatLon>>, budget: Int = 64) =
        StreetContextProjector.projectSegments(0.0, 0.0, ways, 116.0, budget)

    @Test fun denseCenterDoesNotDiscardAllDistantRoads() {
        val center = (0 until 80).map { y -> listOf(geo(174.0, 174.0 + y / 10.0), geo(224.0, 174.0 + y / 10.0)) }
        val corners = listOf(listOf(geo(94.0, 98.0), geo(114.0, 98.0)), listOf(geo(294.0, 298.0), geo(314.0, 298.0)))
        val drawn = segments(center + corners, 64)
        assertTrue(drawn.any { it[0] < 120 })
        assertTrue(drawn.any { it[0] > 280 })
    }

    @Test fun firmwareBudgetAllowsUpToMaxSegments() {
        val ways = (0 until StreetContextProjector.MAX_SEGMENTS).map { index ->
            val x = 84.0 + (index % 16) * 14.0
            val y = 88.0 + (index / 16) * 14.0
            listOf(geo(x, y), geo(x + 10, y))
        }
        assertEquals(StreetContextProjector.MAX_SEGMENTS, segments(ways, StreetContextProjector.MAX_SEGMENTS).size)
    }

    @Test fun roadOutsideTheWindowDoesNotBecomeAnEdgeLine() {
        assertTrue(segments(listOf(listOf(geo(-400.0, 98.0), geo(-390.0, 288.0)))).isEmpty())
    }

    @Test fun lineIsCutAtTheWindowCircle() {
        val line = ScreenGeometry.clip(ScreenPoint(-100.0, c), ScreenPoint(c, c))!!
        assertEquals(ScreenPoint(c - MapProjector.CANVAS_RADIUS, c), line.first)
        assertEquals(ScreenPoint(c, c), line.second)
    }

    @Test fun crossingRoadWithBothEndpointsOutsideIsKept() {
        val lines = segments(listOf(listOf(geo(-400.0, 188.0), geo(800.0, 188.0))))
        assertEquals(1, lines.size)
        assertArrayEquals(intArrayOf(2, 188, 406, 188), lines.single())
    }

    @Test fun closelySpacedNodesBecomeOneContinuousStreet() {
        val way = (104..284).map { geo(it.toDouble(), 188.0) }
        val lines = segments(listOf(way))
        assertEquals(1, lines.size)
        assertArrayEquals(intArrayOf(104, 188, 284, 188), lines.single())
    }

    @Test fun cornerIsPreservedWhenSimplifying() {
        val points = listOf(ScreenPoint(10.0, 10.0), ScreenPoint(50.0, 10.0),
            ScreenPoint(50.0, 60.0))
        assertEquals(points, ScreenGeometry.simplify(points))
    }

    @Test fun allSmallWaysAreKeptWhenTheyFit() {
        val ways = (0 until 50).map { y ->
            listOf(geo(94.0, 88.0 + 2.0 * y), geo(284.0, 88.0 + 2.0 * y))
        }
        assertEquals(50, segments(ways).size)
    }

    @Test fun budgetNeverChopsTheMiddleOfAWay() {
        val bend = listOf(geo(194.0, 198.0), geo(204.0, 198.0), geo(204.0, 228.0))
        assertTrue(segments(listOf(bend), 1).isEmpty())
        assertEquals(2, segments(listOf(bend), 2).size)
    }

    @Test fun outsideExcursionDoesNotDrawAShortcutAcrossTheMap() {
        val points = listOf(ScreenPoint(150.0, 100.0), ScreenPoint(-100.0, 100.0),
            ScreenPoint(-100.0, 300.0), ScreenPoint(150.0, 300.0))
        val runs = ScreenGeometry.visibleRuns(points)
        assertEquals(2, runs.size)
        assertEquals(30.0, runs[0].last().x, 1.0)
        assertEquals(100.0, runs[0].last().y, 0.5)
        assertEquals(25.0, runs[1].first().x, 1.0)
        assertEquals(300.0, runs[1].first().y, 0.5)
        val route = MapProjector.projectRoute(0.0, 0.0,
            points.map { geo(it.x, it.y).let { p -> p.lat to p.lon } }, 116.0)
        assertTrue(route.contains(-1 to -1))
        assertTrue(route.size <= 64)
    }

    @Test fun segmentBudgetIsAlwaysRespected() {
        val ways = (0 until 200).map { y -> listOf(geo(104.0, 88.0 + y), geo(304.0, 88.0 + y)) }
        assertEquals(64, segments(ways).size)
        assertTrue(segments(ways, 0).isEmpty())
    }
}
