package com.vaibhavkishan.motopilot.offline

import com.vaibhavkishan.motopilot.map.MapProjector
import com.vaibhavkishan.motopilot.routing.LatLon
import kotlin.math.*

data class GeoBounds(val south: Double, val west: Double, val north: Double, val east: Double) {
    fun intersects(other: GeoBounds) = south <= other.north && north >= other.south &&
        west <= other.east && east >= other.west
}

data class StreetTile(val x: Int, val y: Int) {
    val key: String get() = "14_${x}_${y}"
    fun bounds(): GeoBounds {
        fun latitude(row: Int) = Math.toDegrees(atan(sinh(PI * (1 - 2.0 * row / SIZE))))
        return GeoBounds(latitude(y + 1), x * 360.0 / SIZE - 180,
            latitude(y), (x + 1) * 360.0 / SIZE - 180)
    }
    companion object { const val SIZE = 16384 }
}

object RouteCoverage {
    const val MAX_TILES = 512
    private const val METERS_PER_DEGREE = 111_320.0
    private const val SAMPLE_SPACING_M = 500.0

    /** Street data must cover the whole rotation window, not just the visible 240x232 area. */
    fun radiusMeters(scale: Double): Double = scale.coerceAtLeast(50.0) * 2.0 *
        MapProjector.CANVAS_RADIUS / MapProjector.MAP_HEIGHT + 100.0

    fun around(point: LatLon, radiusM: Double): GeoBounds {
        if (point.lat !in -80.0..80.0 || point.lon !in -179.0..179.0) {
            throw OfflineMapException(OfflineErrorKind.UNSUPPORTED_REGION, "Region outside the supported coverage")
        }
        val latitudeDelta = radiusM / METERS_PER_DEGREE
        val longitudeDelta = latitudeDelta / cos(Math.toRadians(abs(point.lat) + latitudeDelta))
        return GeoBounds(point.lat - latitudeDelta, point.lon - longitudeDelta,
            point.lat + latitudeDelta, point.lon + longitudeDelta)
    }

    fun tiles(bounds: GeoBounds): List<StreetTile> {
        require(bounds.west >= -180 && bounds.east <= 180 && bounds.south >= -85 && bounds.north <= 85)
        fun x(lon: Double) = floor((lon + 180) / 360 * StreetTile.SIZE).toInt().coerceIn(0, StreetTile.SIZE - 1)
        fun y(lat: Double): Int {
            val rad = Math.toRadians(lat)
            return floor((1 - ln(tan(rad) + 1 / cos(rad)) / PI) / 2 * StreetTile.SIZE)
                .toInt().coerceIn(0, StreetTile.SIZE - 1)
        }
        return buildList {
            for (row in y(bounds.north)..y(bounds.south)) for (col in x(bounds.west)..x(bounds.east)) add(StreetTile(col, row))
        }
    }

    fun plan(route: List<LatLon>, scale: Double): List<StreetTile> {
        require(route.isNotEmpty()) { "Route has no coordinates" }
        val result = linkedSetOf<StreetTile>()
        // The additional half-spacing covers every point between samples, including long straight segments.
        val radius = radiusMeters(scale) + SAMPLE_SPACING_M / 2
        fun include(point: LatLon) {
            result.addAll(tiles(around(point, radius)))
            if (result.size > MAX_TILES) {
                throw OfflineMapException(OfflineErrorKind.ROUTE_TOO_LONG, "Route too long to download at once. Split the trip.")
            }
        }
        include(route.first())
        for ((a, b) in route.zipWithNext()) {
            if (abs(b.lon - a.lon) >= 180) {
                throw OfflineMapException(OfflineErrorKind.UNSUPPORTED_REGION, "Route crosses an unsupported region")
            }
            val distance = hypot((b.lat - a.lat) * METERS_PER_DEGREE,
                (b.lon - a.lon) * METERS_PER_DEGREE * cos(Math.toRadians((a.lat + b.lat) / 2)))
            val count = ceil(distance / SAMPLE_SPACING_M).toInt().coerceAtLeast(1)
            if (count > 10000) {
                throw OfflineMapException(OfflineErrorKind.ROUTE_TOO_LONG, "Route segment too long")
            }
            for (i in 1..count) {
                val t = i.toDouble() / count
                include(LatLon(a.lat + (b.lat - a.lat) * t, a.lon + (b.lon - a.lon) * t))
            }
        }
        return result.toList()
    }
}
