package com.vaibhavkishan.motopilot.navigation

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import kotlin.math.abs

class HeadingFusionTest {
    private fun gap(a: Float, b: Float): Double = abs(((a - b + 540.0) % 360.0) - 180.0)

    @Test
    fun unknownUntilAGpsCourseArrives() {
        val fusion = HeadingFusion()
        fusion.onGyro(10f, 1000)
        fusion.onGyro(10f, 1020)
        assertNull(fusion.heading(1020))
    }

    @Test
    fun slowCoursesAreNotTrusted() {
        val fusion = HeadingFusion()
        fusion.onGps(90f, 1.0f, 1000)
        assertNull(fusion.heading(1000))
    }

    @Test
    fun gyroTurnsTheHeadingBetweenFixes() {
        val fusion = HeadingFusion()
        fusion.onGps(0f, 10f, 0)
        fusion.onGyro(0f, 1)
        var t = 1L
        while (t < 1001) { t += 20; fusion.onGyro(30f, t) }  // 30 deg/s for one second
        assertEquals(30.0, fusion.heading(t)!!.toDouble(), 1.0)
    }

    @Test
    fun wrapsAroundNorth() {
        val fusion = HeadingFusion()
        fusion.onGps(350f, 10f, 0)
        fusion.onGyro(0f, 1)
        var t = 1L
        while (t < 501) { t += 20; fusion.onGyro(40f, t) }
        val heading = fusion.heading(t)!!
        assertEquals(0.0, gap(heading, 10f), 1.5)
    }

    @Test
    fun gpsCourseCorrectsGyroDrift() {
        val fusion = HeadingFusion()
        fusion.onGps(90f, 10f, 0)
        fusion.onGyro(0f, 1)
        var t = 1L
        // The gyro has a bias of 0.5 deg/s; the course says the heading really stays 90.
        repeat(30) {
            repeat(50) { t += 20; fusion.onGyro(0.5f, t) }
            fusion.onGps(90f, 10f, t)
        }
        assertEquals(0.0, gap(fusion.heading(t)!!, 90f), 6.0)
    }

    @Test
    fun aLateCourseDoesNotPullAFastTurnBack() {
        val fusion = HeadingFusion()
        fusion.onGps(0f, 10f, 0)
        fusion.onGyro(0f, 1)
        var t = 1L
        repeat(25) { t += 20; fusion.onGyro(60f, t) }  // quickly into a turn: 30 deg
        val before = fusion.heading(t)!!
        fusion.onGps(5f, 10f, t)  // course still reports the old direction
        assertEquals(0.0, gap(before, fusion.heading(t)!!), 1.0)
    }

    @Test
    fun aHugeGapMeansTheGyroIsLostAndTheCourseWins() {
        val fusion = HeadingFusion()
        fusion.onGps(0f, 10f, 0)
        fusion.onGyro(0f, 1)
        fusion.onGps(120f, 10f, 1000)
        fusion.onGyro(0f, 1000)
        assertEquals(60.0, gap(fusion.heading(1000)!!, 0f), 1.0)
    }

    @Test
    fun forgetsTheHeadingWithoutGpsForTooLong() {
        val fusion = HeadingFusion()
        fusion.onGps(45f, 10f, 0)
        var t = 1L
        fusion.onGyro(0f, t)
        while (t < HeadingFusion.HOLD_MS) { t += 20; fusion.onGyro(0f, t) }
        assertNotNull(fusion.heading(HeadingFusion.HOLD_MS))
        fusion.onGyro(0f, HeadingFusion.HOLD_MS + 1)
        assertNull(fusion.heading(HeadingFusion.HOLD_MS + 1))
    }

    @Test
    fun aQuietGyroFallsBackToTheGpsCourse() {
        val fusion = HeadingFusion()
        fusion.onGps(45f, 10f, 0)
        fusion.onGyro(0f, 1)
        assertNotNull(fusion.heading(400))
        assertNull(fusion.heading(HeadingFusion.GYRO_STALE_MS + 100))
    }

    @Test
    fun resetForgetsEverything() {
        val fusion = HeadingFusion()
        fusion.onGps(45f, 10f, 0)
        fusion.reset()
        assertNull(fusion.heading(10))
    }

    @Test
    fun nonFiniteReadingsAreIgnored() {
        val fusion = HeadingFusion()
        fusion.onGps(45f, 10f, 0)
        fusion.onGyro(Float.NaN, 20)
        fusion.onGps(Float.NaN, 10f, 30)
        assertEquals(45.0, fusion.heading(40)!!.toDouble(), 1e-3)
    }
}
