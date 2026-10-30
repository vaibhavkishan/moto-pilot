package com.vaibhavkishan.motopilot.offline

enum class OfflineErrorKind { ROUTE_TOO_LONG, UNSUPPORTED_REGION, TOO_MUCH_DATA, MAP_SERVER, GENERIC }

/** Offline map failure whose cause the UI can explain without parsing the message. */
class OfflineMapException(val kind: OfflineErrorKind, message: String) : Exception(message)
