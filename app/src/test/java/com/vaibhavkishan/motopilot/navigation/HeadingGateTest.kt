package com.vaibhavkishan.motopilot.navigation

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HeadingGateTest {
    @Test fun stationaryOrMissingCourseIsNotUsed() {
        val gate = HeadingGate()
        assertFalse(gate.isValid(90f, 0f, Float.NaN))
        assertFalse(gate.isValid(Float.NaN, 5f, Float.NaN))
    }

    @Test fun movingWithACourseIsUsed() {
        assertTrue(HeadingGate().isValid(90f, 2f, Float.NaN))
    }

    @Test fun hysteresisKeepsTheHeadingBetweenTheThresholds() {
        val gate = HeadingGate()
        assertTrue(gate.isValid(90f, 2f, Float.NaN))
        assertTrue(gate.isValid(90f, 0.9f, Float.NaN))
        assertFalse(gate.isValid(90f, 0.5f, Float.NaN))
        assertFalse(gate.isValid(90f, 0.9f, Float.NaN))
        assertTrue(gate.isValid(90f, 1.3f, Float.NaN))
    }

    @Test fun losingTheCourseRestartsTheEntryThreshold() {
        val gate = HeadingGate()
        assertTrue(gate.isValid(90f, 2f, Float.NaN))
        assertFalse(gate.isValid(Float.NaN, 2f, Float.NaN))
        assertFalse(gate.isValid(90f, 0.9f, Float.NaN))
    }

    @Test fun poorBearingAccuracyIsRejected() {
        val gate = HeadingGate()
        assertFalse(gate.isValid(90f, 3f, 45f))
        assertTrue(gate.isValid(90f, 3f, 20f))
    }
}
