package com.vaibhavkishan.motopilot.routing

data class PlaceSearchResult(
    val displayName: String,
    val lat: Double,
    val lon: Double,
    val houseNumberUnconfirmed: Boolean = false,
    val source: String = "OpenStreetMap / Nominatim"
)
