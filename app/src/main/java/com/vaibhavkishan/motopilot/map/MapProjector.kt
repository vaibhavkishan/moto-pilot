package com.vaibhavkishan.motopilot.map

import kotlin.math.cos
import kotlin.math.roundToInt

/**
 * Projects lat/lon coordinates to canvas pixels for the CYD map.
 *
 * The CYD shows a 240x232 area but rotates the map itself, so the phone sends a larger square
 * canvas centred on the user. Its circle must cover the farthest corner of the visible area from
 * the on-screen anchor (the user sits in the lower part of the screen), whatever the heading.
 */
object MapProjector {

    const val MAP_WIDTH = 240
    const val MAP_HEIGHT = 232

    /** Must match MAP_ANCHOR_X / MAP_ANCHOR_Y in the CYD firmware. */
    const val ANCHOR_X = 120
    const val ANCHOR_Y = 160

    const val CANVAS_SIZE = 408
    const val CANVAS_CENTER = CANVAS_SIZE / 2.0
    const val CANVAS_RADIUS = 203.0

    private const val METERS_PER_DEG_LAT = 111_320.0

    fun projectRoute(
        centerLat: Double,
        centerLon: Double,
        routePoints: List<Pair<Double, Double>>,
        scaleMeters: Double,
        maxPoints: Int = 64
    ): List<Pair<Int, Int>> {
        val runs = ScreenGeometry.visibleRuns(routePoints.map { (lat, lon) ->
            projectUnclipped(centerLat, centerLon, lat, lon, scaleMeters)
        })
        val result = mutableListOf<Pair<Int, Int>>()
        for (run in runs) {
            val simplified = ScreenGeometry.simplify(run)
            val remaining = maxPoints - result.size - if (result.isEmpty()) 0 else 1
            if (remaining < 2) break
            if (result.isNotEmpty()) result.add(-1 to -1) // Off-screen gap; never draw a shortcut.
            result.addAll(simplified.take(remaining).map { it.x.toInt() to it.y.toInt() })
        }
        return result
    }

    fun projectPoint(
        centerLat: Double,
        centerLon: Double,
        lat: Double,
        lon: Double,
        scaleMeters: Double
    ): Pair<Int, Int> {
        val point = projectUnclipped(centerLat, centerLon, lat, lon, scaleMeters)
        return point.x.roundToInt().coerceIn(0, CANVAS_SIZE - 1) to
            point.y.roundToInt().coerceIn(0, CANVAS_SIZE - 1)
    }

    fun projectUnclipped(
        centerLat: Double, centerLon: Double, lat: Double, lon: Double, scaleMeters: Double
    ): ScreenPoint {
        val metersPerDegLon = METERS_PER_DEG_LAT * cos(Math.toRadians(centerLat))
        val dxMeters = (lon - centerLon) * metersPerDegLon
        val dyMeters = (lat - centerLat) * METERS_PER_DEG_LAT

        val halfW = scaleMeters.coerceAtLeast(50.0)
        // Preserve vertical range and use equal pixel scale on both axes.
        // The scale is defined by the visible map height, independent of the canvas size.
        val pixelsPerMeter = MAP_HEIGHT / (2.0 * halfW)
        return ScreenPoint(CANVAS_CENTER + dxMeters * pixelsPerMeter,
            CANVAS_CENTER - dyMeters * pixelsPerMeter)
    }

    fun simplifyRoute(
        points: List<Pair<Double, Double>>,
        maxPoints: Int = 64
    ): List<Pair<Double, Double>> {
        if (points.size <= maxPoints) return points
        val step = points.size.toDouble() / maxPoints
        return (0 until maxPoints).map { i ->
            points[(i * step).toInt().coerceAtMost(points.lastIndex)]
        }
    }
}
