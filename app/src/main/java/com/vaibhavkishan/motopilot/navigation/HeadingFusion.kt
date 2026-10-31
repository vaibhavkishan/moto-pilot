package com.vaibhavkishan.motopilot.navigation

import kotlin.math.abs

/**
 * Heading between GPS fixes: the gyroscope turns the heading smoothly, the GPS course
 * (about once a second) keeps it from drifting.
 *
 * Gyro readings are clockwise-positive degrees per second about the vertical axis.
 * Safe to call from several threads.
 */
class HeadingFusion {

    private var headingDeg = 0.0
    private var known = false
    private var lastGyroMs = 0L
    private var lastGpsMs = 0L
    private var turnRateDegPerS = 0.0

    @Synchronized
    fun reset() {
        known = false
        lastGyroMs = 0L
        lastGpsMs = 0L
        turnRateDegPerS = 0.0
    }

    @Synchronized
    fun onGyro(yawRateDegPerS: Float, timeMs: Long) {
        if (!yawRateDegPerS.isFinite()) return
        val previous = lastGyroMs
        lastGyroMs = timeMs
        if (previous == 0L || !known) return
        val dt = ((timeMs - previous) / 1000.0).coerceIn(0.0, MAX_GYRO_STEP_S)
        headingDeg = wrap(headingDeg + yawRateDegPerS * dt)
        turnRateDegPerS += (yawRateDegPerS - turnRateDegPerS) * (dt / TURN_RATE_TAU_S).coerceAtMost(1.0)
    }

    /** A GPS course; ignored when it is not a trustworthy direction of travel. */
    @Synchronized
    fun onGps(courseDeg: Float, speedMps: Float, timeMs: Long) {
        if (!courseDeg.isFinite() || !speedMps.isFinite() || speedMps < MIN_SPEED_MPS) return
        lastGpsMs = timeMs
        if (!known) {
            headingDeg = wrap(courseDeg.toDouble())
            known = true
            turnRateDegPerS = 0.0
            return
        }
        val residual = wrapSigned(courseDeg - headingDeg)
        // The GPS course lags while turning, so it is trusted less then; a big gap means the gyro is lost.
        val gain = when {
            abs(residual) > LOST_RESIDUAL_DEG -> LOST_GAIN
            abs(turnRateDegPerS) > TURNING_RATE_DEG_PER_S -> TURNING_GAIN
            else -> STEADY_GAIN
        }
        headingDeg = wrap(headingDeg + residual * gain)
    }

    /**
     * The current heading, or null when there has been no recent GPS course to anchor it or the
     * gyroscope has gone quiet (then the plain GPS course is the better source).
     */
    @Synchronized
    fun heading(nowMs: Long): Float? =
        if (known && nowMs - lastGpsMs <= HOLD_MS && nowMs - lastGyroMs <= GYRO_STALE_MS) headingDeg.toFloat() else null

    private fun wrap(deg: Double): Double = ((deg % 360.0) + 360.0) % 360.0

    private fun wrapSigned(deg: Double): Double = wrap(deg + 180.0) - 180.0

    private fun wrapSigned(deg: Float): Double = wrapSigned(deg.toDouble())

    companion object {
        const val MIN_SPEED_MPS = 2.5f
        const val HOLD_MS = 10_000L
        const val GYRO_STALE_MS = 500L
        private const val MAX_GYRO_STEP_S = 0.2
        private const val TURN_RATE_TAU_S = 0.4
        private const val TURNING_RATE_DEG_PER_S = 15.0
        private const val STEADY_GAIN = 0.12
        private const val TURNING_GAIN = 0.03
        private const val LOST_RESIDUAL_DEG = 45.0
        private const val LOST_GAIN = 0.5
    }
}
