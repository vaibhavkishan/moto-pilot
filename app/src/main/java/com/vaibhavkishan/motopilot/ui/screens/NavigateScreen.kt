package com.vaibhavkishan.motopilot.ui.screens

import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.snap
import androidx.compose.animation.core.tween
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.selection.selectableGroup
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.DirectionsWalk
import androidx.compose.material.icons.filled.BluetoothConnected
import androidx.compose.material.icons.filled.BluetoothDisabled
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.DirectionsCar
import androidx.compose.material.icons.filled.ExpandMore
import androidx.compose.material.icons.filled.GpsFixed
import androidx.compose.material.icons.filled.GpsOff
import androidx.compose.material.icons.filled.PedalBike
import androidx.compose.material.icons.filled.Place
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Sync
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material.icons.filled.TwoWheeler
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.focus.FocusManager
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardCapitalization
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.vaibhavkishan.motopilot.R
import com.vaibhavkishan.motopilot.models.ConnectionState
import com.vaibhavkishan.motopilot.models.CoordinateError
import com.vaibhavkishan.motopilot.models.Coordinates
import com.vaibhavkishan.motopilot.models.MainUiState
import com.vaibhavkishan.motopilot.models.StartBlocker
import com.vaibhavkishan.motopilot.models.startBlocker
import com.vaibhavkishan.motopilot.navigation.GpsStatus
import com.vaibhavkishan.motopilot.routing.PlaceSearchResult
import com.vaibhavkishan.motopilot.routing.RouteProfile
import com.vaibhavkishan.motopilot.ui.components.AppCard
import com.vaibhavkishan.motopilot.ui.components.OfflineMapCard
import com.vaibhavkishan.motopilot.ui.components.PrimaryButton
import com.vaibhavkishan.motopilot.ui.components.ScreenHeader
import com.vaibhavkishan.motopilot.ui.components.SecondaryButton
import com.vaibhavkishan.motopilot.ui.components.SectionTitle
import com.vaibhavkishan.motopilot.ui.components.StatusChip
import com.vaibhavkishan.motopilot.ui.components.TonalButton
import com.vaibhavkishan.motopilot.ui.components.TripProgressCard
import com.vaibhavkishan.motopilot.ui.components.rememberReducedMotion
import com.vaibhavkishan.motopilot.ui.theme.Dimens
import com.vaibhavkishan.motopilot.ui.theme.MotoPilotTheme
import com.vaibhavkishan.motopilot.viewmodel.MainViewModel
import java.util.Locale

@Composable
fun NavigateScreen(
    viewModel: MainViewModel,
    onGoToConnect: () -> Unit,
    onRequestLocationPermission: () -> Unit,
    modifier: Modifier = Modifier
) {
    val ui by viewModel.uiState.collectAsState()
    val trip by viewModel.tripProgress.collectAsState()
    val offlineMap by viewModel.offlineMap.collectAsState()
    val gps by viewModel.gps.collectAsState()
    val focusManager = LocalFocusManager.current
    val connected = ui.connectionState == ConnectionState.CONNECTED
    val blocker = startBlocker(connected, ui.isStarting, ui.destQuery, ui.destLat, ui.destLon)
    var advancedExpanded by rememberSaveable { mutableStateOf(false) }
    val coordinates = Coordinates.resolve(ui.destLat, ui.destLon)

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = Dimens.ScreenPadding),
        verticalArrangement = Arrangement.spacedBy(Dimens.SpaceLg)
    ) {
        Spacer(Modifier.height(Dimens.SpaceSm))
        ScreenHeader(
            title = stringResource(R.string.nav_title),
            subtitle = stringResource(R.string.nav_subtitle)
        )
        DisplayChip(ui.connectionState)
        if (!connected) DisconnectedBanner(navigating = ui.isNavigating, onGoToConnect = onGoToConnect)

        if (ui.isNavigating) {
            ActivePanel(
                ui = ui,
                coordinates = coordinates,
                trip = trip,
                gps = gps,
                offlineMap = offlineMap,
                onRetryOffline = viewModel::retryOfflineDownload,
                onStop = viewModel::stopNavigation
            )
        } else {
            DestinationCard(
                ui = ui,
                coordinates = coordinates,
                focusManager = focusManager,
                onQueryChange = viewModel::updateDestQuery,
                onSearch = { if (!ui.isSearching) viewModel.searchDestination() },
                onSelect = viewModel::selectSearchResult
            )
            RouteTypeCard(ui.routeProfile, enabled = !ui.isStarting, onSelect = viewModel::updateRouteProfile)
            AdvancedCard(
                ui = ui,
                coordinates = coordinates,
                expanded = advancedExpanded,
                onToggle = { advancedExpanded = !advancedExpanded },
                focusManager = focusManager,
                onSearch = { if (!ui.isSearching) viewModel.searchDestination() },
                onCityChange = viewModel::updateSearchCity,
                onLatChange = viewModel::updateDestLat,
                onLonChange = viewModel::updateDestLon
            )
            PrimaryButton(
                text = stringResource(if (ui.isStarting) R.string.starting_navigation else R.string.start_navigation),
                onClick = {
                    focusManager.clearFocus()
                    viewModel.startNavigation(onRequestLocationPermission)
                },
                enabled = blocker == StartBlocker.NONE,
                loading = ui.isStarting
            )
            if (blocker != StartBlocker.NONE) {
                Text(
                    text = stringResource(startHint(blocker)),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.fillMaxWidth()
                )
            }
        }
        Spacer(Modifier.height(Dimens.SpaceSm))
    }
}

private fun startHint(blocker: StartBlocker): Int = when (blocker) {
    StartBlocker.NOT_CONNECTED -> R.string.start_hint_not_connected
    StartBlocker.NO_DESTINATION -> R.string.start_hint_no_destination
    StartBlocker.SEARCH_NOT_RUN -> R.string.start_hint_search_not_run
    StartBlocker.INVALID_COORDINATES -> R.string.start_hint_invalid_coordinates
    StartBlocker.STARTING -> R.string.start_hint_starting
    StartBlocker.NONE -> R.string.start_navigation
}

@Composable
private fun DisplayChip(state: ConnectionState) {
    val scheme = MaterialTheme.colorScheme
    val status = MotoPilotTheme.status
    when (state) {
        ConnectionState.CONNECTED -> StatusChip(
            stringResource(R.string.chip_display_connected), Icons.Filled.BluetoothConnected,
            status.successContainer, status.onSuccessContainer
        )
        ConnectionState.CONNECTING, ConnectionState.RECONNECTING -> StatusChip(
            stringResource(R.string.chip_display_reconnecting), Icons.Filled.Sync,
            scheme.tertiaryContainer, scheme.onTertiaryContainer
        )
        else -> StatusChip(
            stringResource(R.string.chip_display_disconnected), Icons.Filled.BluetoothDisabled,
            scheme.surfaceContainerHigh, scheme.onSurfaceVariant
        )
    }
}

@Composable
private fun DisconnectedBanner(navigating: Boolean, onGoToConnect: () -> Unit) {
    val scheme = MaterialTheme.colorScheme
    AppCard(color = scheme.tertiaryContainer) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)) {
            Icon(Icons.Filled.Warning, null, tint = scheme.onTertiaryContainer, modifier = Modifier.size(24.dp))
            Column(Modifier.weight(1f)) {
                Text(stringResource(R.string.banner_disconnected_title), style = MaterialTheme.typography.titleSmall, color = scheme.onTertiaryContainer)
                Text(
                    stringResource(if (navigating) R.string.active_display_lost else R.string.banner_disconnected_body),
                    style = MaterialTheme.typography.bodyMedium,
                    color = scheme.onTertiaryContainer
                )
            }
        }
        TonalButton(stringResource(R.string.banner_connect), onGoToConnect, Modifier.fillMaxWidth())
    }
}

@Composable
private fun DestinationCard(
    ui: MainUiState,
    coordinates: Pair<Double, Double>?,
    focusManager: FocusManager,
    onQueryChange: (String) -> Unit,
    onSearch: () -> Unit,
    onSelect: (PlaceSearchResult) -> Unit
) {
    val enabled = !ui.isStarting
    AppCard {
        OutlinedTextField(
            value = ui.destQuery,
            onValueChange = onQueryChange,
            modifier = Modifier.fillMaxWidth(),
            enabled = enabled,
            singleLine = true,
            shape = MaterialTheme.shapes.small,
            label = { Text(stringResource(R.string.dest_label)) },
            placeholder = { Text(stringResource(R.string.dest_placeholder)) },
            leadingIcon = { Icon(Icons.Filled.Place, contentDescription = null) },
            trailingIcon = {
                Row {
                    if (ui.destQuery.isNotEmpty()) {
                        IconButton(onClick = { onQueryChange("") }, modifier = Modifier.size(Dimens.MinTouch)) {
                            Icon(Icons.Filled.Close, contentDescription = stringResource(R.string.dest_clear_cd))
                        }
                    }
                    IconButton(onClick = { focusManager.clearFocus(); onSearch() }, enabled = enabled && !ui.isSearching, modifier = Modifier.size(Dimens.MinTouch)) {
                        Icon(Icons.Filled.Search, contentDescription = stringResource(R.string.dest_search_cd))
                    }
                }
            },
            keyboardOptions = KeyboardOptions(capitalization = KeyboardCapitalization.Words, imeAction = ImeAction.Search),
            keyboardActions = KeyboardActions(onSearch = { focusManager.clearFocus(); onSearch() })
        )
        if (ui.isSearching) LinearProgressIndicator(Modifier.fillMaxWidth())

        if (ui.searchResults.isNotEmpty()) {
            SectionTitle(stringResource(R.string.search_results))
            Column {
                ui.searchResults.forEachIndexed { index, result ->
                    if (index > 0) HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
                    ResultRow(result, enabled) { onSelect(result) }
                }
            }
        }

        if (coordinates != null) {
            val lat = String.format(Locale.US, "%.5f", coordinates.first)
            val lon = String.format(Locale.US, "%.5f", coordinates.second)
            Row(verticalAlignment = Alignment.Top, horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)) {
                Icon(Icons.Filled.CheckCircle, null, tint = MotoPilotTheme.status.success, modifier = Modifier.size(24.dp))
                Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                    if (ui.destLabel.isNotBlank()) {
                        Text(
                            stringResource(R.string.dest_selected_summary, ui.destLabel),
                            style = MaterialTheme.typography.bodyLarge,
                            color = MaterialTheme.colorScheme.onSurface
                        )
                        Text("$lat, $lon", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    } else {
                        Text(
                            stringResource(R.string.dest_coords_summary, lat, lon),
                            style = MaterialTheme.typography.bodyLarge,
                            color = MaterialTheme.colorScheme.onSurface
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ResultRow(result: PlaceSearchResult, enabled: Boolean, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 56.dp)
            .clickable(enabled = enabled, role = Role.Button, onClick = onClick)
            .padding(vertical = Dimens.SpaceSm),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)
    ) {
        Icon(Icons.Filled.Place, null, tint = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.size(22.dp))
        Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Text(
                result.displayName,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurface,
                maxLines = 3,
                overflow = TextOverflow.Ellipsis
            )
            if (result.houseNumberUnconfirmed) {
                Text(
                    stringResource(R.string.search_number_unconfirmed),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.tertiary
                )
            }
            Text(result.source, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun RouteTypeCard(selected: RouteProfile, enabled: Boolean, onSelect: (RouteProfile) -> Unit) {
    AppCard {
        SectionTitle(stringResource(R.string.route_type))
        Row(
            modifier = Modifier.fillMaxWidth().selectableGroup(),
            horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceSm)
        ) {
            RouteProfile.entries.forEach { profile ->
                RouteTile(
                    label = stringResource(routeProfileLabel(profile)),
                    icon = routeProfileIcon(profile),
                    selected = profile == selected,
                    enabled = enabled,
                    onClick = { onSelect(profile) },
                    modifier = Modifier.weight(1f)
                )
            }
        }
    }
}

@Composable
private fun RouteTile(
    label: String,
    icon: ImageVector,
    selected: Boolean,
    enabled: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    val scheme = MaterialTheme.colorScheme
    Surface(
        modifier = modifier
            .heightIn(min = 76.dp)
            .selectable(selected = selected, enabled = enabled, role = Role.RadioButton, onClick = onClick),
        shape = MaterialTheme.shapes.small,
        color = if (selected) scheme.primaryContainer else scheme.surfaceContainerHigh,
        border = if (selected) BorderStroke(2.dp, scheme.primary) else null
    ) {
        Column(
            modifier = Modifier.padding(vertical = Dimens.SpaceMd, horizontal = Dimens.SpaceXs),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(Dimens.SpaceXs)
        ) {
            Icon(icon, contentDescription = null, tint = if (selected) scheme.onPrimaryContainer else scheme.onSurfaceVariant)
            Text(
                label,
                style = MaterialTheme.typography.labelMedium,
                color = if (selected) scheme.onPrimaryContainer else scheme.onSurface,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
    }
}

@Composable
private fun AdvancedCard(
    ui: MainUiState,
    coordinates: Pair<Double, Double>?,
    expanded: Boolean,
    onToggle: () -> Unit,
    focusManager: FocusManager,
    onSearch: () -> Unit,
    onCityChange: (String) -> Unit,
    onLatChange: (String) -> Unit,
    onLonChange: (String) -> Unit
) {
    val reducedMotion = rememberReducedMotion()
    val rotation by animateFloatAsState(
        targetValue = if (expanded) 180f else 0f,
        animationSpec = if (reducedMotion) snap() else tween(180),
        label = "advanced-chevron"
    )
    val latError = Coordinates.latitudeError(ui.destLat, ui.destLon)
    val lonError = Coordinates.longitudeError(ui.destLat, ui.destLon)
    val summary = if (coordinates != null) {
        String.format(Locale.US, "%.5f, %.5f", coordinates.first, coordinates.second)
    } else {
        stringResource(R.string.advanced_summary_default)
    }
    val toggleLabel = stringResource(if (expanded) R.string.advanced_collapse else R.string.advanced_expand)

    AppCard {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .heightIn(min = Dimens.MinTouch)
                .clickable(onClickLabel = toggleLabel, role = Role.Button, onClick = onToggle),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)
        ) {
            Icon(Icons.Filled.Tune, null, tint = MaterialTheme.colorScheme.primary)
            Column(Modifier.weight(1f)) {
                Text(stringResource(R.string.advanced_title), style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurface)
                Text(summary, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            Icon(Icons.Filled.ExpandMore, contentDescription = null, modifier = Modifier.rotate(rotation))
        }
        AnimatedVisibility(
            visible = expanded,
            enter = if (reducedMotion) EnterTransition.None else expandVertically() + fadeIn(),
            exit = if (reducedMotion) ExitTransition.None else shrinkVertically() + fadeOut()
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)) {
                OutlinedTextField(
                    value = ui.searchCity,
                    onValueChange = onCityChange,
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true,
                    shape = MaterialTheme.shapes.small,
                    enabled = !ui.isStarting,
                    label = { Text(stringResource(R.string.city_label)) },
                    supportingText = { Text(stringResource(R.string.city_hint)) },
                    keyboardOptions = KeyboardOptions(capitalization = KeyboardCapitalization.Words, imeAction = ImeAction.Search),
                    keyboardActions = KeyboardActions(onSearch = { focusManager.clearFocus(); onSearch() })
                )
                SectionTitle(stringResource(R.string.coords_title))
                CoordinateField(ui.destLat, onLatChange, R.string.lat_label, R.string.lat_placeholder, latError, R.string.coords_error_lat_range, ImeAction.Next, !ui.isStarting)
                CoordinateField(ui.destLon, onLonChange, R.string.lon_label, R.string.lon_placeholder, lonError, R.string.coords_error_lon_range, ImeAction.Done, !ui.isStarting)
                Text(
                    stringResource(R.string.coords_hint),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
    }
}

@Composable
private fun CoordinateField(
    value: String,
    onChange: (String) -> Unit,
    labelRes: Int,
    placeholderRes: Int,
    error: CoordinateError?,
    rangeErrorRes: Int,
    ime: ImeAction,
    enabled: Boolean
) {
    val focusManager = LocalFocusManager.current
    OutlinedTextField(
        value = value,
        onValueChange = onChange,
        modifier = Modifier.fillMaxWidth(),
        singleLine = true,
        enabled = enabled,
        shape = MaterialTheme.shapes.small,
        label = { Text(stringResource(labelRes)) },
        placeholder = { Text(stringResource(placeholderRes)) },
        isError = error != null,
        supportingText = error?.let {
            {
                Text(stringResource(when (it) {
                    CoordinateError.NOT_A_NUMBER -> R.string.coords_error_number
                    CoordinateError.OUT_OF_RANGE -> rangeErrorRes
                    CoordinateError.MISSING -> R.string.coords_error_missing
                }))
            }
        },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal, imeAction = ime),
        keyboardActions = KeyboardActions(onDone = { focusManager.clearFocus() })
    )
}

@Composable
private fun ActivePanel(
    ui: MainUiState,
    coordinates: Pair<Double, Double>?,
    trip: com.vaibhavkishan.motopilot.routing.TripProgress,
    gps: GpsStatus,
    offlineMap: com.vaibhavkishan.motopilot.offline.OfflineMapState,
    onRetryOffline: () -> Unit,
    onStop: () -> Unit
) {
    val destination = ui.destLabel.ifBlank {
        coordinates?.let { String.format(Locale.US, "%.5f, %.5f", it.first, it.second) } ?: ""
    }
    AppCard {
        StatusChip(
            text = stringResource(R.string.active_title),
            icon = Icons.Filled.Sync,
            container = MaterialTheme.colorScheme.primaryContainer,
            content = MaterialTheme.colorScheme.onPrimaryContainer
        )
        Text(
            text = stringResource(R.string.active_to, destination),
            style = MaterialTheme.typography.titleMedium,
            color = MaterialTheme.colorScheme.onSurface,
            maxLines = 3,
            overflow = TextOverflow.Ellipsis
        )
    }
    TripProgressCard(trip)
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceSm)) {
        Icon(
            if (gps.weak) Icons.Filled.GpsOff else Icons.Filled.GpsFixed,
            contentDescription = null,
            tint = if (gps.weak) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.size(18.dp)
        )
        Text(
            stringResource(if (gps.weak) R.string.gps_weak else R.string.gps_accuracy, gps.accuracyM?.toInt() ?: 0),
            style = MaterialTheme.typography.bodySmall,
            color = if (gps.weak) MaterialTheme.colorScheme.tertiary else MaterialTheme.colorScheme.onSurfaceVariant
        )
    }
    OfflineMapCard(offlineMap, onRetryOffline)
    SecondaryButton(
        text = stringResource(R.string.stop_navigation),
        onClick = onStop,
        modifier = Modifier.fillMaxWidth(),
        destructive = true
    )
}

private fun routeProfileLabel(profile: RouteProfile): Int = when (profile) {
    RouteProfile.CAR -> R.string.route_profile_car
    RouteProfile.MOTORCYCLE -> R.string.route_profile_motorcycle
    RouteProfile.BIKE -> R.string.route_profile_bike
    RouteProfile.WALKING -> R.string.route_profile_walking
}

private fun routeProfileIcon(profile: RouteProfile): ImageVector = when (profile) {
    RouteProfile.CAR -> Icons.Filled.DirectionsCar
    RouteProfile.MOTORCYCLE -> Icons.Filled.TwoWheeler
    RouteProfile.BIKE -> Icons.Filled.PedalBike
    RouteProfile.WALKING -> Icons.AutoMirrored.Filled.DirectionsWalk
}
