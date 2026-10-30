package com.vaibhavkishan.motopilot.models

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class UiLogicTest {
    @Test fun connectionPhaseNeverContradicts() {
        assertEquals(ConnectionPhase.CONNECTED, connectionPhase(ConnectionState.CONNECTED, true))
        assertEquals(ConnectionPhase.CONNECTING, connectionPhase(ConnectionState.CONNECTING, true))
        assertEquals(ConnectionPhase.RECONNECTING, connectionPhase(ConnectionState.RECONNECTING, false))
        assertEquals(ConnectionPhase.FAILED, connectionPhase(ConnectionState.ERROR, false))
        assertEquals(ConnectionPhase.SCANNING, connectionPhase(ConnectionState.ERROR, true))
        assertEquals(ConnectionPhase.SCANNING, connectionPhase(ConnectionState.DISCONNECTED, true))
        assertEquals(ConnectionPhase.NOT_CONNECTED, connectionPhase(ConnectionState.DISCONNECTED, false))
    }

    @Test fun signalBuckets() {
        assertEquals(SignalLevel.STRONG, signalLevel(-45))
        assertEquals(SignalLevel.STRONG, signalLevel(-60))
        assertEquals(SignalLevel.FAIR, signalLevel(-61))
        assertEquals(SignalLevel.FAIR, signalLevel(-75))
        assertEquals(SignalLevel.WEAK, signalLevel(-90))
    }

    @Test fun coordinatesAcceptPointsAndCommas() {
        assertEquals(12.9366924 to 77.7228489, Coordinates.resolve("12.9366924", "77.7228489"))
        assertEquals(12.93 to 77.72, Coordinates.resolve(" 12,93 ", "77,72"))
    }

    @Test fun coordinatesRejectInvalidInputBeforeNavigationSeesIt() {
        assertNull(Coordinates.resolve("", ""))
        assertNull(Coordinates.resolve("abc", "77"))
        assertNull(Coordinates.resolve("91", "77"))
        assertNull(Coordinates.resolve("12", "-181"))
        assertNull(Coordinates.resolve("NaN", "10"))
        assertNull(Coordinates.resolve("Infinity", "10"))
        assertEquals(90.0 to -180.0, Coordinates.resolve("90", "-180"))
    }

    @Test fun fieldErrorsPointAtTheRightField() {
        assertNull(Coordinates.latitudeError("", ""))
        assertEquals(CoordinateError.MISSING, Coordinates.longitudeError("12.9", ""))
        assertEquals(CoordinateError.MISSING, Coordinates.latitudeError("", "77.7"))
        assertEquals(CoordinateError.NOT_A_NUMBER, Coordinates.latitudeError("1x", "77.7"))
        assertEquals(CoordinateError.OUT_OF_RANGE, Coordinates.latitudeError("95", "77.7"))
        assertEquals(CoordinateError.OUT_OF_RANGE, Coordinates.longitudeError("12", "200"))
        assertNull(Coordinates.latitudeError("12", "200"))
    }

    @Test fun startBlockersAreReportedInOrder() {
        assertEquals(StartBlocker.STARTING, startBlocker(true, true, "x", "1", "2"))
        assertEquals(StartBlocker.NOT_CONNECTED, startBlocker(false, false, "", "", ""))
        assertEquals(StartBlocker.NO_DESTINATION, startBlocker(true, false, " ", "", ""))
        assertEquals(StartBlocker.SEARCH_NOT_RUN, startBlocker(true, false, "cafe", "", ""))
        assertEquals(StartBlocker.INVALID_COORDINATES, startBlocker(true, false, "cafe", "95", "10"))
        assertEquals(StartBlocker.INVALID_COORDINATES, startBlocker(true, false, "", "12", ""))
        assertEquals(StartBlocker.NONE, startBlocker(true, false, "", "12.9", "77.7"))
    }
}
