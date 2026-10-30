package com.vaibhavkishan.motopilot.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.stringResource
import com.vaibhavkishan.motopilot.R
import com.vaibhavkishan.motopilot.routing.EtaFormatter
import com.vaibhavkishan.motopilot.routing.TripProgress
import com.vaibhavkishan.motopilot.ui.theme.Dimens

@Composable
fun TripProgressCard(progress: TripProgress, modifier: Modifier = Modifier) {
    val locale = LocalConfiguration.current.locales[0]
    val distance = progress.remainingDistanceM?.let {
        String.format(locale, if (it < 1000) "%.2f km" else "%.1f km", it / 1000.0)
    } ?: "—"
    val time = progress.remainingDurationS?.let {
        val minutes = (it + 59) / 60
        if (minutes >= 60) "${minutes / 60} h ${minutes % 60} min" else "$minutes min"
    } ?: "—"
    val arrival = if (progress.offRoute) null else EtaFormatter.format(System.currentTimeMillis(), progress.remainingDurationS)
    AppCard(modifier) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceLg)) {
            Metric(stringResource(R.string.remaining_time), time, Modifier.weight(1f))
            Metric(stringResource(R.string.remaining_distance), distance, Modifier.weight(1f))
        }
        if (arrival != null) {
            Text(
                text = stringResource(R.string.trip_arrival, arrival),
                style = MaterialTheme.typography.titleSmall,
                color = MaterialTheme.colorScheme.onSurface
            )
        }
        Text(
            text = stringResource(
                if (progress.offRoute) R.string.trip_off_route
                else if (progress.remainingDistanceM == null) R.string.trip_calculating else R.string.trip_estimate
            ),
            style = MaterialTheme.typography.bodySmall,
            color = if (progress.offRoute) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.onSurfaceVariant
        )
    }
}

@Composable
private fun Metric(label: String, value: String, modifier: Modifier = Modifier) {
    Column(modifier) {
        Text(label, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, style = MaterialTheme.typography.headlineMedium, color = MaterialTheme.colorScheme.primary)
    }
}
