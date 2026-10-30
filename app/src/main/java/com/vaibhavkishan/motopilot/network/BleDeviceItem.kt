package com.vaibhavkishan.motopilot.network

data class BleDeviceItem(
    val address: String,
    val name: String,
    val rssi: Int
)
