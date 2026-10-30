package com.vaibhavkishan.motopilot.routing

/** Splits a route at the matched position into the travelled part and the part still ahead. */
object RouteSplit {

    data class Parts(val done: List<LatLon>, val ahead: List<LatLon>)

    fun split(points: List<LatLon>, segmentIndex: Int?, matched: LatLon?): Parts {
        if (segmentIndex == null || matched == null || segmentIndex !in 0 until points.lastIndex) {
            return Parts(emptyList(), points)
        }
        return Parts(
            done = points.subList(0, segmentIndex + 1) + matched,
            ahead = listOf(matched) + points.subList(segmentIndex + 1, points.size)
        )
    }
}
