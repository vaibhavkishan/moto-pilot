package com.vaibhavkishan.motopilot.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import com.vaibhavkishan.motopilot.R
import com.vaibhavkishan.motopilot.models.TransmissionStats
import com.vaibhavkishan.motopilot.ui.theme.Dimens
import kotlin.math.roundToInt

/** Live transmission numbers as a 2x2 grid of label/value pairs. */
@Composable
fun StatsCard(stats: TransmissionStats, modifier: Modifier = Modifier) {
    AppCard(modifier) {
        SectionTitle(stringResource(R.string.stats_title))
        Text(
            stringResource(R.string.stats_hint),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceLg)) {
            Stat(stringResource(R.string.stat_updates), String.format("%.1f/s", stats.updatesPerSecond), Modifier.weight(1f))
            Stat(stringResource(R.string.stat_bitrate), formatRate(stats.bytesPerSecond), Modifier.weight(1f))
        }
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceLg)) {
            Stat(stringResource(R.string.stat_resolution), stats.resolution, Modifier.weight(1f))
            Stat(stringResource(R.string.stat_total_updates), stats.totalUpdates.toString(), Modifier.weight(1f))
        }
    }
}

@Composable
private fun Stat(label: String, value: String, modifier: Modifier = Modifier) {
    Column(modifier) {
        Text(label, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(value, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurface)
    }
}

private fun formatRate(bytesPerSecond: Long): String = when {
    bytesPerSecond >= 1024 -> String.format("%.1f KB/s", bytesPerSecond / 1024.0)
    else -> "$bytesPerSecond B/s"
}
