package com.vaibhavkishan.motopilot

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.core.content.ContextCompat
import com.vaibhavkishan.motopilot.ui.MotoPilotApp
import com.vaibhavkishan.motopilot.ui.theme.MotoPilotTheme
import com.vaibhavkishan.motopilot.viewmodel.MainViewModel

class MainActivity : ComponentActivity() {

    private val mainViewModel: MainViewModel by viewModels()

    private val locationPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { grants ->
        val granted = grants[Manifest.permission.ACCESS_FINE_LOCATION] == true ||
            grants[Manifest.permission.ACCESS_COARSE_LOCATION] == true
        if (granted) {
            mainViewModel.onLocationPermissionGranted()
        } else {
            mainViewModel.onLocationPermissionDenied()
        }
    }

    private val bluetoothPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { grants ->
        val granted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            grants[Manifest.permission.BLUETOOTH_SCAN] == true &&
                grants[Manifest.permission.BLUETOOTH_CONNECT] == true
        } else {
            grants[Manifest.permission.ACCESS_FINE_LOCATION] == true ||
                grants[Manifest.permission.ACCESS_COARSE_LOCATION] == true
        }
        if (granted) {
            mainViewModel.onBluetoothPermissionGranted()
        } else {
            mainViewModel.onBluetoothPermissionDenied()
        }
    }

    private val notificationPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { /* continue even if denied */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestNotificationPermissionIfNeeded()
        enableEdgeToEdge()

        setContent {
            MotoPilotTheme {
                MotoPilotApp(
                    viewModel = mainViewModel,
                    onRequestLocationPermission = ::requestLocationPermission,
                    onRequestBluetoothPermission = ::requestBluetoothPermission
                )
            }
        }
    }

    private fun requestLocationPermission() {
        locationPermissionLauncher.launch(
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            )
        )
    }

    private fun requestBluetoothPermission() {
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            )
        }
        bluetoothPermissionLauncher.launch(permissions)
    }

    private fun requestNotificationPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(
                    this,
                    Manifest.permission.POST_NOTIFICATIONS
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
        }
    }
}
