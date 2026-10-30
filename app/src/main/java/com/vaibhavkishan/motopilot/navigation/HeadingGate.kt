package com.vaibhavkishan.motopilot.navigation

/**
 * Decides when a GPS course can be trusted as the direction of travel for map rotation.
 *
 * Android only reports a meaningful course while moving. Entering needs a higher speed than
 * leaving, so a rider hovering around the threshold does not flip the map back and forth.
 */
class HeadingGate(
    private val enterSpeedMps: Float = 1.2f,
    private val exitSpeedMps: Float = 0.7f,
    private val maxAccuracyDeg: Float = 30f
) {
    private var moving = false

    fun reset() {
        moving = false
    }

    /** [accuracyDeg] is NaN when the device does not report bearing accuracy. */
    fun isValid(bearing: Float, speedMps: Float, accuracyDeg: Float): Boolean {
        if (!bearing.isFinite() || !speedMps.isFinite()) {
            moving = false
            return false
        }
        moving = speedMps >= if (moving) exitSpeedMps else enterSpeedMps
        return moving && (accuracyDeg.isNaN() || accuracyDeg <= maxAccuracyDeg)
    }
}
