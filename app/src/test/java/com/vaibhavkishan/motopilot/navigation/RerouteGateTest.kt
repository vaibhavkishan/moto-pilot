package com.vaibhavkishan.motopilot.navigation

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class RerouteGateTest {
    @Test fun leavingTheRouteBrieflyDoesNotReroute() {
        val gate = RerouteGate()
        assertFalse(gate.shouldStart(true, 0))
        assertFalse(gate.shouldStart(true, 5_999))
        assertFalse(gate.shouldStart(false, 6_000))
        assertFalse(gate.shouldStart(true, 6_500))   // timer restarted when back on the route
        assertTrue(gate.shouldStart(true, 12_500))
    }

    @Test fun onlyOneRequestRunsAtATime() {
        val gate = RerouteGate()
        gate.shouldStart(true, 0)
        assertTrue(gate.shouldStart(true, 6_000))
        assertFalse(gate.shouldStart(true, 7_000))
        assertFalse(gate.shouldStart(true, 60_000))
    }

    @Test fun failuresBackOffAndSuccessResets() {
        val gate = RerouteGate()
        gate.shouldStart(true, 0)
        assertTrue(gate.shouldStart(true, 6_000))
        gate.onFailure(7_000)                        // retry after 15 s
        assertFalse(gate.shouldStart(true, 21_999))
        assertTrue(gate.shouldStart(true, 22_000))
        gate.onFailure(23_000)                       // then 30 s
        assertFalse(gate.shouldStart(true, 52_999))
        assertTrue(gate.shouldStart(true, 53_000))
        gate.onSuccess()
        assertFalse(gate.shouldStart(true, 54_000))  // a fresh deviation must last again
        assertTrue(gate.shouldStart(true, 60_000))
    }

    @Test fun resetForgetsEverything() {
        val gate = RerouteGate()
        gate.shouldStart(true, 0)
        gate.shouldStart(true, 6_000)
        gate.reset()
        assertFalse(gate.shouldStart(true, 6_100))
        assertTrue(gate.shouldStart(true, 12_100))
    }
}
