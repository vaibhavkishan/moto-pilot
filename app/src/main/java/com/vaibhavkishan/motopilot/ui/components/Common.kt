package com.vaibhavkishan.motopilot.ui.components

import android.provider.Settings
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.vaibhavkishan.motopilot.ui.theme.Dimens

/** True when the phone's animation scale is 0, so transitions should be skipped. */
@Composable
fun rememberReducedMotion(): Boolean {
    val context = LocalContext.current
    return remember {
        Settings.Global.getFloat(context.contentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 1f) == 0f
    }
}

@Composable
fun ScreenHeader(title: String, modifier: Modifier = Modifier, subtitle: String? = null) {
    Column(modifier = modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(Dimens.SpaceXs)) {
        Text(
            text = title,
            style = MaterialTheme.typography.headlineMedium,
            color = MaterialTheme.colorScheme.onSurface,
            modifier = Modifier.semantics { heading() }
        )
        if (subtitle != null) {
            Text(
                text = subtitle,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
fun SectionTitle(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text,
        style = MaterialTheme.typography.titleMedium,
        color = MaterialTheme.colorScheme.onSurface,
        modifier = modifier.semantics { heading() }
    )
}

/** Flat tonal surface used for every grouped block; never nested inside another card. */
@Composable
fun AppCard(
    modifier: Modifier = Modifier,
    contentPadding: PaddingValues = PaddingValues(Dimens.SpaceLg),
    color: Color = MaterialTheme.colorScheme.surfaceContainer,
    content: @Composable ColumnScope.() -> Unit
) {
    Surface(modifier = modifier.fillMaxWidth(), shape = MaterialTheme.shapes.medium, color = color) {
        Column(
            modifier = Modifier.padding(contentPadding),
            verticalArrangement = Arrangement.spacedBy(Dimens.SpaceMd),
            content = content
        )
    }
}

@Composable
private fun ButtonLabel(text: String, icon: ImageVector?, loading: Boolean) {
    if (loading) {
        CircularProgressIndicator(
            modifier = Modifier.size(20.dp),
            strokeWidth = 2.dp,
            color = LocalContentColor.current
        )
        Spacer(Modifier.width(Dimens.SpaceSm))
    } else if (icon != null) {
        Icon(icon, contentDescription = null, modifier = Modifier.size(20.dp))
        Spacer(Modifier.width(Dimens.SpaceSm))
    }
    Text(text, style = MaterialTheme.typography.labelLarge)
}

@Composable
fun PrimaryButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    loading: Boolean = false,
    icon: ImageVector? = null
) {
    Button(
        onClick = onClick,
        enabled = enabled && !loading,
        modifier = modifier.fillMaxWidth().heightIn(min = Dimens.ButtonHeight),
        shape = MaterialTheme.shapes.small
    ) { ButtonLabel(text, icon, loading) }
}

@Composable
fun TonalButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    icon: ImageVector? = null
) {
    FilledTonalButton(
        onClick = onClick,
        enabled = enabled,
        modifier = modifier.heightIn(min = Dimens.ButtonHeight),
        shape = MaterialTheme.shapes.small
    ) { ButtonLabel(text, icon, loading = false) }
}

@Composable
fun SecondaryButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    icon: ImageVector? = null,
    destructive: Boolean = false
) {
    val color = if (destructive) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.primary
    OutlinedButton(
        onClick = onClick,
        enabled = enabled,
        modifier = modifier.heightIn(min = Dimens.ButtonHeight),
        shape = MaterialTheme.shapes.small,
        colors = ButtonDefaults.outlinedButtonColors(contentColor = color)
    ) { ButtonLabel(text, icon, loading = false) }
}

/** Icon plus text, so status never relies on colour alone. */
@Composable
fun StatusChip(
    text: String,
    icon: ImageVector,
    container: Color,
    content: Color,
    modifier: Modifier = Modifier
) {
    Surface(modifier = modifier, shape = CircleShape, color = container) {
        Row(
            modifier = Modifier.padding(horizontal = Dimens.SpaceMd, vertical = Dimens.SpaceSm - 2.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            Icon(icon, contentDescription = null, tint = content, modifier = Modifier.size(16.dp))
            Text(text, style = MaterialTheme.typography.labelMedium, color = content)
        }
    }
}

/** App mark: blue rounded tile with the white navigation arrow (same as the launcher icon and the CYD). */
@Composable
fun AppLogo(size: Dp, modifier: Modifier = Modifier) {
    val blue = Color(0xFF1A73E8)
    Canvas(modifier = modifier.size(size)) {
        val s = this.size.minDimension
        drawRoundRect(color = blue, cornerRadius = CornerRadius(s * 0.24f))
        val k = s * 0.50f / 24f
        val cx = s / 2f
        val cy = s / 2f
        val arrow = Path().apply {
            moveTo(cx, cy - 12 * k)
            lineTo(cx - 10 * k, cy + 12 * k)
            lineTo(cx, cy + 6 * k)
            lineTo(cx + 10 * k, cy + 12 * k)
            close()
        }
        drawPath(arrow, Color.White)
    }
}
