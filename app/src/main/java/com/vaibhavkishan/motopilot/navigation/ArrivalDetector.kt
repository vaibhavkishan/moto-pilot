package com.vaibhavkishan.motopilot.navigation

import com.vaibhavkishan.motopilot.routing.RouteProfile

/** Reports arrival once the remaining route distance stays within the threshold on consecutive updates. */
class ArrivalDetector(private val requiredHits: Int = 2) {
    private var hits = 0

    fun reset() { hits = 0 }

    fun update(remainingM: Int?, offRoute: Boolean, thresholdM: Int): Boolean {
        if (offRoute || remainingM == null || remainingM > thresholdM) {
            hits = 0
            return false
        }
        hits++
        return hits >= requiredHits
    }

    companion object {
        fun thresholdM(profile: RouteProfile): Int = when (profile) {
            RouteProfile.WALKING -> 15
            RouteProfile.BIKE -> 20
            else -> 30
        }
    }
}
