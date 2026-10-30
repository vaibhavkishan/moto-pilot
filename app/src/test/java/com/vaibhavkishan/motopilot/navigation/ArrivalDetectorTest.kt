package com.vaibhavkishan.motopilot.navigation

import com.vaibhavkishan.motopilot.routing.RouteProfile
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ArrivalDetectorTest {
    @Test
    fun needsConsecutiveUpdatesInsideTheThreshold() {
        val detector = ArrivalDetector(requiredHits = 2)
        assertFalse(detector.update(10, offRoute = false, thresholdM = 20))
        assertTrue(detector.update(8, offRoute = false, thresholdM = 20))
    }

    @Test
    fun aFarReadingResetsTheCount() {
        val detector = ArrivalDetector(requiredHits = 2)
        assertFalse(detector.update(10, false, 20))
        assertFalse(detector.update(120, false, 20))
        assertFalse(detector.update(10, false, 20))
        assertTrue(detector.update(10, false, 20))
    }

    @Test
    fun offRouteOrUnknownDistanceNeverArrives() {
        val detector = ArrivalDetector(requiredHits = 1)
        assertFalse(detector.update(5, offRoute = true, thresholdM = 20))
        assertFalse(detector.update(null, offRoute = false, thresholdM = 20))
        assertTrue(detector.update(5, offRoute = false, thresholdM = 20))
    }

    @Test
    fun walkingUsesATighterThresholdThanCars() {
        assertEquals(15, ArrivalDetector.thresholdM(RouteProfile.WALKING))
        assertEquals(30, ArrivalDetector.thresholdM(RouteProfile.CAR))
    }
}
