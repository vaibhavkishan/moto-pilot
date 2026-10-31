package com.vaibhavkishan.motopilot.models

import com.vaibhavkishan.motopilot.routing.RouteProfile

data class AppSettings(
    val bleDeviceAddress: String = "",
    val bleDeviceName: String = "",
    val navUpdateHz: Int = 2,
    val mapScaleMeters: Double = 400.0,
    val routeProfile: RouteProfile = RouteProfile.CAR,
    val mapStyle: MapStyle = MapStyle.CLASSIC,
    val zoomEnabled: Boolean = true,
    val zoomMax: Double = 3.5,
    val zoomLeadSeconds: Int = 8,
    val destQuery: String = "",
    val destLabel: String = "",
    val destLat: String = "",
    val destLon: String = ""
) {
    val resolutionLabel: String get() = "240×232 map"
}
