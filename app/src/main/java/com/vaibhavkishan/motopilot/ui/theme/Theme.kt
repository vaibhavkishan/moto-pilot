package com.vaibhavkishan.motopilot.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.unit.dp

/** Spacing scale and sizes shared by every screen. */
object Dimens {
    val ScreenPadding = 16.dp
    val SpaceXs = 4.dp
    val SpaceSm = 8.dp
    val SpaceMd = 12.dp
    val SpaceLg = 16.dp
    val SpaceXl = 24.dp
    val ButtonHeight = 52.dp
    val MinTouch = 48.dp
}

private val AppShapes = Shapes(
    extraSmall = RoundedCornerShape(8.dp),
    small = RoundedCornerShape(12.dp),
    medium = RoundedCornerShape(16.dp),
    large = RoundedCornerShape(24.dp),
    extraLarge = RoundedCornerShape(28.dp)
)

private val LocalStatusColors = staticCompositionLocalOf { LightStatusColors }

object MotoPilotTheme {
    val status: StatusColors
        @Composable @ReadOnlyComposable get() = LocalStatusColors.current
}

/** Follows the phone's light/dark setting. */
@Composable
fun MotoPilotTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit
) {
    CompositionLocalProvider(LocalStatusColors provides if (darkTheme) DarkStatusColors else LightStatusColors) {
        MaterialTheme(
            colorScheme = if (darkTheme) DarkColors else LightColors,
            typography = Typography,
            shapes = AppShapes,
            content = content
        )
    }
}
