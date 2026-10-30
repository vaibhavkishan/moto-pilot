package com.vaibhavkishan.motopilot.ui.theme

import androidx.compose.material3.ColorScheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Immutable
import androidx.compose.ui.graphics.Color

/** Status colours that Material 3 does not define (success). Warning uses `tertiary`, error uses `error`. */
@Immutable
data class StatusColors(
    val success: Color,
    val onSuccess: Color,
    val successContainer: Color,
    val onSuccessContainer: Color
)

internal val LightStatusColors = StatusColors(
    success = Color(0xFF1E8E3E),
    onSuccess = Color(0xFFFFFFFF),
    successContainer = Color(0xFFE6F4EA),
    onSuccessContainer = Color(0xFF0D652D)
)

internal val DarkStatusColors = StatusColors(
    success = Color(0xFF81C995),
    onSuccess = Color(0xFF0B2A14),
    successContainer = Color(0xFF16301E),
    onSuccessContainer = Color(0xFFA8DAB5)
)

internal val LightColors: ColorScheme = lightColorScheme(
    primary = Color(0xFF1A73E8),
    onPrimary = Color(0xFFFFFFFF),
    primaryContainer = Color(0xFFD2E3FC),
    onPrimaryContainer = Color(0xFF0B3A8A),
    secondary = Color(0xFF5F6368),
    onSecondary = Color(0xFFFFFFFF),
    secondaryContainer = Color(0xFFD2E3FC),
    onSecondaryContainer = Color(0xFF0B3A8A),
    tertiary = Color(0xFF9A5B00),
    onTertiary = Color(0xFFFFFFFF),
    tertiaryContainer = Color(0xFFFEEFC3),
    onTertiaryContainer = Color(0xFF5C3A00),
    background = Color(0xFFF8F9FA),
    onBackground = Color(0xFF202124),
    surface = Color(0xFFF8F9FA),
    onSurface = Color(0xFF202124),
    surfaceVariant = Color(0xFFEEF1F4),
    onSurfaceVariant = Color(0xFF5F6368),
    surfaceContainerLowest = Color(0xFFFFFFFF),
    surfaceContainerLow = Color(0xFFFFFFFF),
    surfaceContainer = Color(0xFFF1F3F4),
    surfaceContainerHigh = Color(0xFFE8EAED),
    surfaceContainerHighest = Color(0xFFDADCE0),
    outline = Color(0xFF80868B),
    outlineVariant = Color(0xFFDADCE0),
    error = Color(0xFFD93025),
    onError = Color(0xFFFFFFFF),
    errorContainer = Color(0xFFFCE8E6),
    onErrorContainer = Color(0xFF8C1D18)
)

internal val DarkColors: ColorScheme = darkColorScheme(
    primary = Color(0xFF8AB4F8),
    onPrimary = Color(0xFF062E6F),
    primaryContainer = Color(0xFF1F3F7A),
    onPrimaryContainer = Color(0xFFD6E4FF),
    secondary = Color(0xFFA8B3C7),
    onSecondary = Color(0xFF1A2333),
    secondaryContainer = Color(0xFF1F3F7A),
    onSecondaryContainer = Color(0xFFD6E4FF),
    tertiary = Color(0xFFFDD663),
    onTertiary = Color(0xFF3B2F00),
    tertiaryContainer = Color(0xFF4A3B00),
    onTertiaryContainer = Color(0xFFFEEFC3),
    background = Color(0xFF0E1420),
    onBackground = Color(0xFFE8EAED),
    surface = Color(0xFF0E1420),
    onSurface = Color(0xFFE8EAED),
    surfaceVariant = Color(0xFF1C2639),
    onSurfaceVariant = Color(0xFFA8B3C7),
    surfaceContainerLowest = Color(0xFF0A0F19),
    surfaceContainerLow = Color(0xFF121A29),
    surfaceContainer = Color(0xFF162033),
    surfaceContainerHigh = Color(0xFF1C2639),
    surfaceContainerHighest = Color(0xFF243049),
    outline = Color(0xFF7C8AA5),
    outlineVariant = Color(0xFF2E3A52),
    error = Color(0xFFF28B82),
    onError = Color(0xFF601410),
    errorContainer = Color(0xFF5C1A17),
    onErrorContainer = Color(0xFFFAD2CF)
)
