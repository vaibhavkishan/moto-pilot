package com.vaibhavkishan.motopilot.models

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class CoordinateSearchTextTest {
    @Test fun commonPairFormatsAreCoordinates() {
        val expected = -23.5505 to -46.6333
        assertEquals(expected, Coordinates.fromSearchText("-23.5505, -46.6333"))
        assertEquals(expected, Coordinates.fromSearchText("-23.5505,-46.6333"))
        assertEquals(expected, Coordinates.fromSearchText("  -23.5505 -46.6333 "))
        assertEquals(expected, Coordinates.fromSearchText("(-23.5505, -46.6333)"))
        assertEquals(expected, Coordinates.fromSearchText("-23.5505; -46.6333"))
        assertEquals(12.93 to 77.72, Coordinates.fromSearchText("12,93 77,72"))
    }

    @Test fun hemisphereLettersSetTheSign() {
        assertEquals(-23.55 to -46.63, Coordinates.fromSearchText("23.55 S, 46.63 W"))
        assertEquals(23.55 to 46.63, Coordinates.fromSearchText("23.55°N 46.63°E"))
    }

    @Test fun placesAndOtherTextAreStillSearched() {
        assertNull(Coordinates.fromSearchText("Avenida Paulista 1578"))
        assertNull(Coordinates.fromSearchText("Rua Augusta"))
        assertNull(Coordinates.fromSearchText("10 20"))
        assertNull(Coordinates.fromSearchText("12.5"))
        assertNull(Coordinates.fromSearchText("12.5, 77.5, 3"))
        assertNull(Coordinates.fromSearchText(""))
    }

    @Test fun outOfRangeNumbersAreNotCoordinates() {
        assertNull(Coordinates.fromSearchText("95.0, 10.0"))
        assertNull(Coordinates.fromSearchText("10.0, 190.0"))
    }

    @Test fun wholeNumberPairsWithACommaAreCoordinates() {
        assertEquals(10.0 to 20.0, Coordinates.fromSearchText("10, 20"))
    }
}
