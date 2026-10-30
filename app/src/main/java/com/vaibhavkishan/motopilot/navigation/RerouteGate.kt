package com.vaibhavkishan.motopilot.navigation

import kotlin.math.min

/**
 * Decides when to ask for a new route after leaving the planned one.
 *
 * Leaving the route must last [sustainMs] (so a GPS glitch does not trigger it), only one request
 * runs at a time, and failed requests back off so a missing connection is not hammered.
 */
class RerouteGate(
    private val sustainMs: Long = 6_000,
    private val retryDelaysMs: LongArray = longArrayOf(15_000, 30_000, 60_000)
) {
    private var offSinceMs: Long? = null
    private var inFlight = false
    private var notBeforeMs = 0L
    private var failures = 0

    @Synchronized fun reset() {
        offSinceMs = null
        inFlight = false
        notBeforeMs = 0
        failures = 0
    }

    /** Call with every position update. Returns true when a reroute request should start now. */
    @Synchronized fun shouldStart(offRoute: Boolean, nowMs: Long): Boolean {
        if (!offRoute) {
            offSinceMs = null
            return false
        }
        val since = offSinceMs ?: nowMs.also { offSinceMs = it }
        if (inFlight || nowMs < notBeforeMs || nowMs - since < sustainMs) return false
        inFlight = true
        return true
    }

    @Synchronized fun onSuccess() {
        inFlight = false
        failures = 0
        offSinceMs = null
    }

    @Synchronized fun onFailure(nowMs: Long) {
        inFlight = false
        notBeforeMs = nowMs + retryDelaysMs[min(failures, retryDelaysMs.lastIndex)]
        failures++
    }
}
