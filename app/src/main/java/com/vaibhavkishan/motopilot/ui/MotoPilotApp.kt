package com.vaibhavkishan.motopilot.ui

import androidx.compose.animation.EnterTransition
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.Navigation
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Badge
import androidx.compose.material3.BadgedBox
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import com.vaibhavkishan.motopilot.R
import com.vaibhavkishan.motopilot.models.ConnectionState
import com.vaibhavkishan.motopilot.ui.components.rememberReducedMotion
import com.vaibhavkishan.motopilot.ui.screens.ConnectScreen
import com.vaibhavkishan.motopilot.ui.screens.NavigateScreen
import com.vaibhavkishan.motopilot.ui.screens.SettingsScreen
import com.vaibhavkishan.motopilot.viewmodel.MainViewModel
import kotlinx.coroutines.launch

object Routes {
    const val CONNECT = "connect"
    const val NAVIGATE = "navigate"
    const val SETTINGS = "settings"
}

private data class Tab(val route: String, val labelRes: Int, val icon: ImageVector)

private val tabs = listOf(
    Tab(Routes.CONNECT, R.string.tab_connect, Icons.Filled.Bluetooth),
    Tab(Routes.NAVIGATE, R.string.tab_navigate, Icons.Filled.Navigation),
    Tab(Routes.SETTINGS, R.string.tab_settings, Icons.Filled.Settings)
)

@Composable
fun MotoPilotApp(
    viewModel: MainViewModel,
    onRequestLocationPermission: () -> Unit,
    onRequestBluetoothPermission: () -> Unit
) {
    val navController = rememberNavController()
    val uiState by viewModel.uiState.collectAsState()
    val snackbarHostState = remember { SnackbarHostState() }
    val scope = rememberCoroutineScope()
    val context = LocalContext.current
    val reducedMotion = rememberReducedMotion()
    val currentRoute = navController.currentBackStackEntryAsState().value?.destination?.route

    val selectTab: (String) -> Unit = { route ->
        navController.navigate(route) {
            popUpTo(navController.graph.findStartDestination().id) { saveState = true }
            launchSingleTop = true
            restoreState = true
        }
    }
    val showMessage: (String) -> Unit = { message -> scope.launch { snackbarHostState.showSnackbar(message) } }

    LaunchedEffect(uiState.errorMessage) {
        uiState.errorMessage?.let { message ->
            snackbarHostState.showSnackbar(message)
            viewModel.clearError()
        }
    }

    LaunchedEffect(Unit) {
        viewModel.arrivals.collect { snackbarHostState.showSnackbar(context.getString(R.string.msg_arrived)) }
    }

    // Report the outcome of a connection attempt once, when the state changes.
    var previous by remember { mutableStateOf(uiState.connectionState) }
    LaunchedEffect(uiState.connectionState) {
        val now = uiState.connectionState
        if (now == ConnectionState.CONNECTED && previous != ConnectionState.CONNECTED) {
            val name = uiState.bleDeviceName.ifBlank { uiState.bleDeviceAddress }
            snackbarHostState.showSnackbar(context.getString(R.string.msg_connected, name))
        } else if (now == ConnectionState.ERROR && previous != ConnectionState.ERROR) {
            snackbarHostState.showSnackbar(context.getString(R.string.msg_connect_failed))
        }
        previous = now
    }

    Scaffold(
        containerColor = MaterialTheme.colorScheme.background,
        snackbarHost = { SnackbarHost(snackbarHostState) },
        bottomBar = {
            NavigationBar {
                tabs.forEach { tab ->
                    val label = stringResource(tab.labelRes)
                    NavigationBarItem(
                        selected = currentRoute == tab.route,
                        onClick = { selectTab(tab.route) },
                        icon = {
                            if (tab.route == Routes.NAVIGATE && uiState.isNavigating) {
                                BadgedBox(badge = { Badge() }) { Icon(tab.icon, contentDescription = null) }
                            } else {
                                Icon(tab.icon, contentDescription = null)
                            }
                        },
                        label = { Text(label) }
                    )
                }
            }
        }
    ) { padding ->
        val enter = if (reducedMotion) EnterTransition.None else fadeIn(tween(160))
        val exit = if (reducedMotion) ExitTransition.None else fadeOut(tween(120))
        NavHost(
            navController = navController,
            startDestination = Routes.CONNECT,
            modifier = Modifier
                .padding(padding)
                .consumeWindowInsets(padding)
                .imePadding(),
            enterTransition = { enter },
            exitTransition = { exit },
            popEnterTransition = { enter },
            popExitTransition = { exit }
        ) {
            composable(Routes.CONNECT) {
                ConnectScreen(
                    viewModel = viewModel,
                    onRequestBluetoothPermission = onRequestBluetoothPermission,
                    onGoToNavigate = { selectTab(Routes.NAVIGATE) },
                    onShowMessage = showMessage
                )
            }
            composable(Routes.NAVIGATE) {
                NavigateScreen(
                    viewModel = viewModel,
                    onGoToConnect = { selectTab(Routes.CONNECT) },
                    onRequestLocationPermission = onRequestLocationPermission
                )
            }
            composable(Routes.SETTINGS) { SettingsScreen() }
        }
    }
}
