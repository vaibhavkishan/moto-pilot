package com.vaibhavkishan.motopilot.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Map
import androidx.compose.material.icons.filled.Navigation
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.stateDescription
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.vaibhavkishan.motopilot.R
import com.vaibhavkishan.motopilot.ui.components.AppCard
import com.vaibhavkishan.motopilot.ui.components.AppLogo
import com.vaibhavkishan.motopilot.ui.components.ScreenHeader
import com.vaibhavkishan.motopilot.ui.components.SectionTitle
import com.vaibhavkishan.motopilot.ui.components.StatsCard
import com.vaibhavkishan.motopilot.ui.theme.Dimens
import com.vaibhavkishan.motopilot.viewmodel.SettingsViewModel

@Composable
fun SettingsScreen(
    modifier: Modifier = Modifier,
    viewModel: SettingsViewModel = viewModel()
) {
    val settings by viewModel.settings.collectAsState()
    val stats by viewModel.stats.collectAsState()
    val context = LocalContext.current

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = Dimens.ScreenPadding),
        verticalArrangement = Arrangement.spacedBy(Dimens.SpaceLg)
    ) {
        Spacer(Modifier.height(Dimens.SpaceSm))
        ScreenHeader(stringResource(R.string.settings_title))

        SettingsSection(Icons.Filled.Navigation, stringResource(R.string.settings_navigation_section)) {
            SliderSetting(
                title = stringResource(R.string.setting_nav_hz_title),
                format = { context.getString(R.string.setting_nav_hz_value, it.toInt()) },
                value = settings.navUpdateHz.toFloat(),
                valueRange = 1f..5f,
                steps = 3,
                minLabel = stringResource(R.string.setting_nav_hz_value, 1),
                maxLabel = stringResource(R.string.setting_nav_hz_value, 5),
                description = stringResource(R.string.setting_nav_hz_desc),
                onValueChange = { value -> viewModel.updateSettings { it.copy(navUpdateHz = value.toInt()) } }
            )
        }

        SettingsSection(Icons.Filled.Map, stringResource(R.string.settings_map_section)) {
            SliderSetting(
                title = stringResource(R.string.setting_map_scale_title),
                format = { context.getString(R.string.setting_map_scale_value, it.toInt()) },
                value = settings.mapScaleMeters.toFloat(),
                valueRange = 150f..800f,
                steps = 12,
                minLabel = stringResource(R.string.setting_map_scale_value, 150),
                maxLabel = stringResource(R.string.setting_map_scale_value, 800),
                description = stringResource(R.string.setting_map_scale_desc),
                onValueChange = { value -> viewModel.updateSettings { it.copy(mapScaleMeters = value.toDouble()) } }
            )
        }

        StatsCard(stats = stats.copy(resolution = settings.resolutionLabel))
        AboutCard()
        Spacer(Modifier.height(Dimens.SpaceSm))
    }
}

@Composable
private fun SettingsSection(icon: ImageVector, title: String, content: @Composable () -> Unit) {
    AppCard {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)) {
            Surface(shape = CircleShape, color = MaterialTheme.colorScheme.primaryContainer, modifier = Modifier.size(40.dp)) {
                Box(contentAlignment = Alignment.Center) {
                    Icon(icon, null, tint = MaterialTheme.colorScheme.onPrimaryContainer, modifier = Modifier.size(22.dp))
                }
            }
            SectionTitle(title)
        }
        content()
    }
}

@Composable
private fun SliderSetting(
    title: String,
    format: (Float) -> String,
    value: Float,
    valueRange: ClosedFloatingPointRange<Float>,
    steps: Int,
    minLabel: String,
    maxLabel: String,
    description: String,
    onValueChange: (Float) -> Unit
) {
    var position by remember(value) { mutableFloatStateOf(value) }
    val valueText = format(position)
    Column(verticalArrangement = Arrangement.spacedBy(Dimens.SpaceXs)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(title, style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurface, modifier = Modifier.weight(1f))
            Text(valueText, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.primary)
        }
        Slider(
            value = position,
            onValueChange = { position = it; onValueChange(it) },
            valueRange = valueRange,
            steps = steps,
            modifier = Modifier.semantics { stateDescription = valueText }
        )
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(minLabel, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(maxLabel, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        Text(description, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
private fun AboutCard() {
    val context = LocalContext.current
    val uriHandler = LocalUriHandler.current
    val version = remember {
        runCatching { context.packageManager.getPackageInfo(context.packageName, 0).versionName }.getOrNull().orEmpty()
    }
    AppCard {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)) {
            AppLogo(size = 40.dp)
            Column {
                SectionTitle(stringResource(R.string.app_name))
                if (version.isNotBlank()) {
                    Text(
                        stringResource(R.string.about_version, version),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        }
        Text(
            stringResource(R.string.about_credits),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Text(
            stringResource(R.string.about_data),
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        TextButton(onClick = { uriHandler.openUri("https://www.openstreetmap.org/fixthemap") }) {
            Text(stringResource(R.string.fix_map))
        }
    }
}
