package com.vaibhavkishan.motopilot.routing

import org.junit.Assert.assertEquals
import org.junit.Test
import java.util.TimeZone

class ManeuverAndEtaTest {
    private fun icon(type: String, modifier: String = "") =
        ManeuverIcon.fromInstruction(InstructionText.format(type, modifier))

    @Test fun osrmManeuversMapToIcons() {
        assertEquals("depart", icon("depart", "right"))
        assertEquals("arrive", icon("arrive", "left"))
        assertEquals("right", icon("turn", "right"))
        assertEquals("left", icon("end of road", "left"))
        assertEquals("slight_right", icon("fork", "slight right"))
        assertEquals("slight_left", icon("fork", "slight left"))
        assertEquals("sharp_right", icon("turn", "sharp right"))
        assertEquals("sharp_left", icon("turn", "sharp left"))
        assertEquals("uturn", icon("turn", "uturn"))
        assertEquals("straight", icon("continue", "straight"))
        assertEquals("straight", icon("new name"))
    }

    @Test fun roundaboutsAreRecognisedWhateverTheirModifier() {
        assertEquals("roundabout", icon("roundabout", "right"))
        assertEquals("roundabout", icon("rotary", "left"))
        assertEquals("roundabout", icon("roundabout turn", "slight right"))
        assertEquals("roundabout", icon("exit roundabout", "right"))
    }

    @Test fun etaIsTheClockTimeOfArrival() {
        val utc = TimeZone.getTimeZone("UTC")
        assertEquals("01:00", EtaFormatter.format(0L, 3600, utc))
        assertEquals("14:32", EtaFormatter.format(14 * 3_600_000L + 2 * 60_000L, 30 * 60, utc))
        assertEquals("00:05", EtaFormatter.format(23 * 3_600_000L + 55 * 60_000L, 10 * 60, utc))
    }

    @Test fun etaUsesTheLocalTimeZone() {
        assertEquals("05:30", EtaFormatter.format(0L, 0, TimeZone.getTimeZone("Asia/Kolkata")))
    }

    @Test fun unknownOrInvalidDurationHasNoEta() {
        assertEquals(null, EtaFormatter.format(0L, null))
        assertEquals(null, EtaFormatter.format(0L, -1))
    }
}
