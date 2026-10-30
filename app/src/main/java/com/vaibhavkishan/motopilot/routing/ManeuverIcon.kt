package com.vaibhavkishan.motopilot.routing

/** Maneuver text shown on the CYD, built from OSRM maneuver type and modifier. */
object InstructionText {
    const val START = "Start route"
    const val ARRIVE = "Destination ahead"
    const val TURN_RIGHT = "Turn right"
    const val TURN_LEFT = "Turn left"
    const val SLIGHT_RIGHT = "Slight right"
    const val SLIGHT_LEFT = "Slight left"
    const val SHARP_RIGHT = "Sharp right"
    const val SHARP_LEFT = "Sharp left"
    const val STRAIGHT = "Continue straight"
    const val U_TURN = "Make a U-turn"
    const val ENTER_ROUNDABOUT = "Enter roundabout"
    const val EXIT_ROUNDABOUT = "Exit roundabout"

    fun format(type: String, modifier: String): String = when {
        type == "arrive" -> ARRIVE
        type == "depart" -> START
        type == "roundabout" || type == "rotary" || type == "roundabout turn" -> ENTER_ROUNDABOUT
        type == "exit roundabout" || type == "exit rotary" -> EXIT_ROUNDABOUT
        modifier == "uturn" -> U_TURN
        modifier == "right" -> TURN_RIGHT
        modifier == "left" -> TURN_LEFT
        modifier == "slight right" -> SLIGHT_RIGHT
        modifier == "slight left" -> SLIGHT_LEFT
        modifier == "sharp right" -> SHARP_RIGHT
        modifier == "sharp left" -> SHARP_LEFT
        modifier == "straight" -> STRAIGHT
        modifier.isNotBlank() -> modifier.replaceFirstChar { it.uppercase() }
        else -> type.replaceFirstChar { it.uppercase() }
    }
}

/** Icon codes understood by the CYD firmware (`maneuver` field of the nav message). */
object ManeuverIcon {
    fun fromInstruction(instruction: String): String = when (instruction) {
        InstructionText.START -> "depart"
        InstructionText.ARRIVE -> "arrive"
        InstructionText.TURN_RIGHT -> "right"
        InstructionText.TURN_LEFT -> "left"
        InstructionText.SLIGHT_RIGHT -> "slight_right"
        InstructionText.SLIGHT_LEFT -> "slight_left"
        InstructionText.SHARP_RIGHT -> "sharp_right"
        InstructionText.SHARP_LEFT -> "sharp_left"
        InstructionText.U_TURN -> "uturn"
        InstructionText.ENTER_ROUNDABOUT, InstructionText.EXIT_ROUNDABOUT -> "roundabout"
        else -> "straight"
    }
}
