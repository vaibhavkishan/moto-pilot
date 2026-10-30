package com.vaibhavkishan.motopilot.routing

import java.util.Calendar
import java.util.Locale
import java.util.TimeZone

/** Arrival clock time (24 h) from the remaining duration. */
object EtaFormatter {
    fun format(nowMs: Long, remainingS: Int?, timeZone: TimeZone = TimeZone.getDefault()): String? {
        if (remainingS == null || remainingS < 0) return null
        val arrival = Calendar.getInstance(timeZone).apply { timeInMillis = nowMs + remainingS * 1000L }
        return String.format(Locale.US, "%02d:%02d", arrival.get(Calendar.HOUR_OF_DAY), arrival.get(Calendar.MINUTE))
    }
}
