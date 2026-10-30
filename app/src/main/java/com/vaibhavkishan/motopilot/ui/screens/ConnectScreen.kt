package com.vaibhavkishan.motopilot.ui.screens

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
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
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.BluetoothConnected
import androidx.compose.material.icons.filled.BluetoothDisabled
import androidx.compose.material.icons.filled.BluetoothSearching
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Error
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.SearchOff
import androidx.compose.material.icons.filled.SignalCellularAlt
import androidx.compose.material.icons.filled.SignalCellularAlt1Bar
import androidx.compose.material.icons.filled.SignalCellularAlt2Bar
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.vaibhavkishan.motopilot.R
import com.vaibhavkishan.motopilot.models.ConnectionPhase
import com.vaibhavkishan.motopilot.models.SignalLevel
import com.vaibhavkishan.motopilot.models.connectionPhase
import com.vaibhavkishan.motopilot.models.signalLevel
import com.vaibhavkishan.motopilot.network.BleDeviceItem
import com.vaibhavkishan.motopilot.ui.components.AppCard
import com.vaibhavkishan.motopilot.ui.components.AppLogo
import com.vaibhavkishan.motopilot.ui.components.PrimaryButton
import com.vaibhavkishan.motopilot.ui.components.ScreenHeader
import com.vaibhavkishan.motopilot.ui.components.SecondaryButton
import com.vaibhavkishan.motopilot.ui.components.SectionTitle
import com.vaibhavkishan.motopilot.ui.components.StatusChip
import com.vaibhavkishan.motopilot.ui.theme.Dimens
import com.vaibhavkishan.motopilot.ui.theme.MotoPilotTheme
import com.vaibhavkishan.motopilot.viewmodel.MainViewModel

@Composable
fun ConnectScreen(
    viewModel: MainViewModel,
    onRequestBluetoothPermission: () -> Unit,
    onGoToNavigate: () -> Unit,
    onShowMessage: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    val ui by viewModel.uiState.collectAsState()
    val context = LocalContext.current
    val phase = connectionPhase(ui.connectionState, ui.isScanning)
    val savedName = ui.bleDeviceName.ifBlank { ui.bleDeviceAddress }
    val hasSavedDevice = ui.bleDeviceAddress.isNotBlank()
    var hasScanned by rememberSaveable { mutableStateOf(false) }
    LaunchedEffect(ui.isScanning) { if (ui.isScanning) hasScanned = true }

    val toggleScan = {
        if (ui.isScanning) viewModel.stopBleScan() else viewModel.startBleScan(onRequestBluetoothPermission)
    }
    val connectSaved = { viewModel.connectSavedDevice(onRequestBluetoothPermission) }
    val forget = {
        viewModel.forgetDevice()
        onShowMessage(context.getString(R.string.msg_forgotten))
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = Dimens.ScreenPadding),
        verticalArrangement = Arrangement.spacedBy(Dimens.SpaceLg)
    ) {
        Spacer(Modifier.height(Dimens.SpaceSm))
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)) {
            AppLogo(size = 36.dp)
            Text(
                text = stringResource(R.string.app_name),
                style = MaterialTheme.typography.titleLarge,
                color = MaterialTheme.colorScheme.onSurface
            )
        }
        ScreenHeader(
            title = stringResource(R.string.connect_title),
            subtitle = stringResource(R.string.connect_subtitle)
        )

        ConnectionStatusCard(
            phase = phase,
            deviceName = savedName,
            hasSavedDevice = hasSavedDevice,
            onScan = toggleScan,
            onConnectSaved = connectSaved,
            onDisconnect = viewModel::disconnect,
            onForget = forget,
            onGoToNavigate = onGoToNavigate
        )

        if (phase != ConnectionPhase.CONNECTED) {
            DevicesCard(
                devices = ui.scannedDevices,
                savedAddress = ui.bleDeviceAddress,
                scanning = ui.isScanning,
                hasScanned = hasScanned,
                busy = phase == ConnectionPhase.CONNECTING || phase == ConnectionPhase.RECONNECTING,
                onRescan = toggleScan,
                onConnect = { viewModel.connectToDevice(it, onRequestBluetoothPermission) }
            )
            TipRow(stringResource(R.string.tip_pairing))
        }
        Spacer(Modifier.height(Dimens.SpaceSm))
    }
}

@Composable
private fun ConnectionStatusCard(
    phase: ConnectionPhase,
    deviceName: String,
    hasSavedDevice: Boolean,
    onScan: () -> Unit,
    onConnectSaved: () -> Unit,
    onDisconnect: () -> Unit,
    onForget: () -> Unit,
    onGoToNavigate: () -> Unit
) {
    val status = MotoPilotTheme.status
    val scheme = MaterialTheme.colorScheme
    val container: Color
    val onContainer: Color
    when (phase) {
        ConnectionPhase.CONNECTED -> { container = status.successContainer; onContainer = status.onSuccessContainer }
        ConnectionPhase.FAILED -> { container = scheme.errorContainer; onContainer = scheme.onErrorContainer }
        else -> { container = scheme.surfaceContainer; onContainer = scheme.onSurface }
    }
    val title = stringResource(when (phase) {
        ConnectionPhase.CONNECTED -> R.string.status_connected
        ConnectionPhase.CONNECTING -> R.string.status_connecting
        ConnectionPhase.RECONNECTING -> R.string.status_reconnecting
        ConnectionPhase.SCANNING -> R.string.status_searching
        ConnectionPhase.FAILED -> R.string.status_failed
        ConnectionPhase.NOT_CONNECTED -> R.string.status_not_connected
    })
    val hint = stringResource(when (phase) {
        ConnectionPhase.CONNECTED -> R.string.status_connected_hint
        ConnectionPhase.CONNECTING, ConnectionPhase.RECONNECTING -> R.string.status_connecting_hint
        ConnectionPhase.SCANNING -> R.string.status_searching_hint
        ConnectionPhase.FAILED -> R.string.status_failed_hint
        ConnectionPhase.NOT_CONNECTED -> R.string.status_not_connected_hint
    })

    AppCard(
        color = container,
        modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite }
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)) {
            Box(
                modifier = Modifier.size(48.dp),
                contentAlignment = Alignment.Center
            ) {
                when (phase) {
                    ConnectionPhase.CONNECTING, ConnectionPhase.RECONNECTING, ConnectionPhase.SCANNING ->
                        CircularProgressIndicator(modifier = Modifier.size(32.dp), strokeWidth = 3.dp, color = onContainer)
                    ConnectionPhase.CONNECTED -> Icon(Icons.Filled.BluetoothConnected, null, tint = onContainer, modifier = Modifier.size(32.dp))
                    ConnectionPhase.FAILED -> Icon(Icons.Filled.Error, null, tint = onContainer, modifier = Modifier.size(32.dp))
                    ConnectionPhase.NOT_CONNECTED -> Icon(Icons.Filled.BluetoothDisabled, null, tint = onContainer, modifier = Modifier.size(32.dp))
                }
            }
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(title, style = MaterialTheme.typography.titleMedium, color = onContainer)
                if (deviceName.isNotBlank() && phase != ConnectionPhase.SCANNING) {
                    Text(
                        text = if (phase == ConnectionPhase.NOT_CONNECTED) stringResource(R.string.last_used, deviceName) else deviceName,
                        style = MaterialTheme.typography.bodyLarge,
                        color = onContainer,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis
                    )
                }
                Text(hint, style = MaterialTheme.typography.bodyMedium, color = onContainer)
            }
        }

        when (phase) {
            ConnectionPhase.CONNECTED -> {
                PrimaryButton(stringResource(R.string.action_go_navigate), onGoToNavigate)
                Row(horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)) {
                    SecondaryButton(stringResource(R.string.action_disconnect), onDisconnect, Modifier.weight(1f), destructive = true)
                    SecondaryButton(stringResource(R.string.action_forget), onForget, Modifier.weight(1f))
                }
            }
            ConnectionPhase.CONNECTING, ConnectionPhase.RECONNECTING ->
                SecondaryButton(stringResource(R.string.action_cancel), onDisconnect, Modifier.fillMaxWidth())
            ConnectionPhase.SCANNING ->
                SecondaryButton(stringResource(R.string.action_stop_scan), onScan, Modifier.fillMaxWidth())
            ConnectionPhase.FAILED -> {
                PrimaryButton(
                    text = stringResource(R.string.action_try_again),
                    onClick = if (hasSavedDevice) onConnectSaved else onScan
                )
                if (hasSavedDevice) SecondaryButton(stringResource(R.string.action_forget), onForget, Modifier.fillMaxWidth())
            }
            ConnectionPhase.NOT_CONNECTED -> {
                if (hasSavedDevice) {
                    PrimaryButton(stringResource(R.string.action_connect_to, deviceName), onConnectSaved)
                    Row(horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)) {
                        SecondaryButton(stringResource(R.string.action_scan), onScan, Modifier.weight(1f))
                        SecondaryButton(stringResource(R.string.action_forget), onForget, Modifier.weight(1f))
                    }
                } else {
                    PrimaryButton(stringResource(R.string.action_scan), onScan)
                }
            }
        }
    }
}

@Composable
private fun DevicesCard(
    devices: List<BleDeviceItem>,
    savedAddress: String,
    scanning: Boolean,
    hasScanned: Boolean,
    busy: Boolean,
    onRescan: () -> Unit,
    onConnect: (BleDeviceItem) -> Unit
) {
    AppCard(contentPadding = androidx.compose.foundation.layout.PaddingValues(vertical = Dimens.SpaceSm)) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(start = Dimens.SpaceLg, end = Dimens.SpaceXs),
            verticalAlignment = Alignment.CenterVertically
        ) {
            SectionTitle(stringResource(R.string.devices_title), Modifier.weight(1f))
            IconButton(onClick = onRescan, modifier = Modifier.size(Dimens.MinTouch)) {
                if (scanning) {
                    CircularProgressIndicator(modifier = Modifier.size(22.dp), strokeWidth = 2.dp)
                } else {
                    Icon(Icons.Filled.Refresh, contentDescription = stringResource(R.string.action_rescan))
                }
            }
        }
        when {
            devices.isNotEmpty() -> devices.forEachIndexed { index, device ->
                if (index > 0) HorizontalDivider(Modifier.padding(horizontal = Dimens.SpaceLg), color = MaterialTheme.colorScheme.outlineVariant)
                DeviceRow(device, isSaved = device.address == savedAddress, enabled = !busy, onClick = { onConnect(device) })
            }
            scanning -> EmptyMessage(stringResource(R.string.devices_scanning), null, showSpinner = true)
            hasScanned -> EmptyMessage(
                title = stringResource(R.string.devices_empty_title),
                body = stringResource(R.string.devices_empty_hint),
                icon = Icons.Filled.SearchOff
            )
            else -> EmptyMessage(stringResource(R.string.devices_before_scan), null)
        }
    }
}

@Composable
private fun EmptyMessage(title: String, body: String?, icon: ImageVector? = null, showSpinner: Boolean = false) {
    Column(
        modifier = Modifier.fillMaxWidth().padding(horizontal = Dimens.SpaceLg, vertical = Dimens.SpaceMd),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(Dimens.SpaceSm)
    ) {
        if (icon != null) Icon(icon, null, tint = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.size(32.dp))
        if (showSpinner) CircularProgressIndicator(modifier = Modifier.size(24.dp), strokeWidth = 2.dp)
        Text(title, style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.onSurface)
        if (body != null) {
            Text(
                body,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                textAlign = androidx.compose.ui.text.style.TextAlign.Center
            )
        }
    }
}

@Composable
private fun DeviceRow(device: BleDeviceItem, isSaved: Boolean, enabled: Boolean, onClick: () -> Unit) {
    val level = signalLevel(device.rssi)
    val levelText = stringResource(when (level) {
        SignalLevel.STRONG -> R.string.device_signal_strong
        SignalLevel.FAIR -> R.string.device_signal_fair
        SignalLevel.WEAK -> R.string.device_signal_weak
    })
    val rssiText = stringResource(R.string.device_signal_format, device.rssi)
    val action = stringResource(R.string.device_row_action, device.name)
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 64.dp)
            .clickable(enabled = enabled, onClickLabel = action, role = Role.Button, onClick = onClick)
            .padding(horizontal = Dimens.SpaceLg, vertical = Dimens.SpaceSm)
            .semantics(mergeDescendants = true) { contentDescription = "${device.name}, $levelText, $rssiText" },
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceMd)
    ) {
        Surface(shape = CircleShape, color = MaterialTheme.colorScheme.primaryContainer, modifier = Modifier.size(40.dp)) {
            Box(contentAlignment = Alignment.Center) {
                Icon(Icons.Filled.Bluetooth, null, tint = MaterialTheme.colorScheme.onPrimaryContainer, modifier = Modifier.size(22.dp))
            }
        }
        Column(modifier = Modifier.weight(1f)) {
            Text(device.name, style = MaterialTheme.typography.bodyLarge, maxLines = 1, overflow = TextOverflow.Ellipsis,
                color = MaterialTheme.colorScheme.onSurface)
            Text(device.address, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
        if (isSaved) {
            StatusChip(
                text = stringResource(R.string.device_saved),
                icon = Icons.Filled.Check,
                container = MaterialTheme.colorScheme.secondaryContainer,
                content = MaterialTheme.colorScheme.onSecondaryContainer
            )
        }
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Icon(
                imageVector = when (level) {
                    SignalLevel.STRONG -> Icons.Filled.SignalCellularAlt
                    SignalLevel.FAIR -> Icons.Filled.SignalCellularAlt2Bar
                    SignalLevel.WEAK -> Icons.Filled.SignalCellularAlt1Bar
                },
                contentDescription = null,
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.size(20.dp)
            )
            Text(rssiText, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun TipRow(text: String) {
    Row(
        modifier = Modifier.fillMaxWidth().padding(horizontal = Dimens.SpaceXs),
        horizontalArrangement = Arrangement.spacedBy(Dimens.SpaceSm),
        verticalAlignment = Alignment.Top
    ) {
        Icon(Icons.Filled.Info, null, tint = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.size(18.dp))
        Text(text, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}
