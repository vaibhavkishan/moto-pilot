package com.vaibhavkishan.motopilot.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.size
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CloudDone
import androidx.compose.material.icons.filled.CloudDownload
import androidx.compose.material.icons.filled.CloudOff
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import com.vaibhavkishan.motopilot.R
import com.vaibhavkishan.motopilot.offline.OfflineErrorKind
import com.vaibhavkishan.motopilot.offline.OfflineMapState
import com.vaibhavkishan.motopilot.ui.theme.Dimens
import com.vaibhavkishan.motopilot.ui.theme.MotoPilotTheme

@Composable
fun OfflineMapCard(state: OfflineMapState, onRetry: () -> Unit, modifier: Modifier = Modifier) {
    val failed = state.error != null
    val titleRes = when {
        state.ready && state.outsideCoverage -> R.string.offline_outside
        state.ready -> R.string.offline_ready
        failed -> R.string.offline_incomplete
        else -> R.string.offline_downloading
    }
    val icon = when {
        failed || (state.ready && state.outsideCoverage) -> Icons.Filled.CloudOff
        state.ready -> Icons.Filled.CloudDone
        else -> Icons.Filled.CloudDownload
    }
    val tint = when {
        failed -> MaterialTheme.colorScheme.error
        state.ready && state.outsideCoverage -> MaterialTheme.colorScheme.tertiary
        state.ready -> MotoPilotTheme.status.success
        else -> MaterialTheme.colorScheme.primary
    }
    val errorRes = when (state.errorKind) {
        OfflineErrorKind.ROUTE_TOO_LONG -> R.string.offline_too_long
        OfflineErrorKind.UNSUPPORTED_REGION -> R.string.offline_unsupported
        OfflineErrorKind.TOO_MUCH_DATA -> R.string.offline_too_dense
        OfflineErrorKind.MAP_SERVER -> R.string.offline_server_busy
        else -> R.string.offline_download_error
    }
    val hintRes = when {
        failed -> errorRes
        state.ready && !state.outsideCoverage -> R.string.offline_ready_hint
        state.ready -> R.string.offline_outside_hint
        else -> R.string.offline_wait_hint
    }

    AppCard(modifier) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)) {
            Icon(icon, contentDescription = null, tint = tint, modifier = Modifier.size(24.dp))
            Text(stringResource(titleRes), style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurface)
        }
        if (state.total > 0) {
            LinearProgressIndicator(progress = { state.downloaded.toFloat() / state.total }, modifier = Modifier.fillMaxWidth())
            Text(
                stringResource(R.string.offline_progress, state.downloaded, state.total, state.radiusM),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        } else if (!failed) {
            LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
        }
        Text(stringResource(hintRes), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        if (failed) {
            SecondaryButton(stringResource(R.string.offline_retry), onRetry, enabled = !state.downloading)
        }
    }
}
