package com.vaibhavkishan.motopilot.navigation

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.SystemClock
import kotlin.math.sqrt

/** Source of the turn rate about the vertical axis, whatever way the phone is mounted. */
interface YawRateSource {
    /** Calls [onRate] with clockwise-positive degrees per second and a SystemClock.elapsedRealtime time in ms. */
    fun start(onRate: (Float, Long) -> Unit): Boolean

    fun stop()
}

/**
 * Projects the gyroscope onto the gravity direction, so the yaw rate is right for a phone
 * in a handlebar mount at any angle.
 */
class YawRateSensor(context: Context) : YawRateSource, SensorEventListener {

    private val sensorManager = context.applicationContext.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private var onRate: ((Float, Long) -> Unit)? = null
    private val up = FloatArray(3)
    private var haveGravity = false

    override fun start(onRate: (Float, Long) -> Unit): Boolean {
        stop()
        val gyro = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE) ?: return false
        val gravity = sensorManager.getDefaultSensor(Sensor.TYPE_GRAVITY) ?: return false
        this.onRate = onRate
        haveGravity = false
        sensorManager.registerListener(this, gravity, SensorManager.SENSOR_DELAY_GAME)
        sensorManager.registerListener(this, gyro, SensorManager.SENSOR_DELAY_GAME)
        return true
    }

    override fun stop() {
        sensorManager.unregisterListener(this)
        onRate = null
        haveGravity = false
    }

    override fun onSensorChanged(event: SensorEvent) {
        when (event.sensor.type) {
            Sensor.TYPE_GRAVITY -> {
                val norm = sqrt(
                    event.values[0] * event.values[0] + event.values[1] * event.values[1] + event.values[2] * event.values[2]
                )
                if (norm < 1f) return
                for (i in 0..2) up[i] = event.values[i] / norm
                haveGravity = true
            }
            Sensor.TYPE_GYROSCOPE -> {
                if (!haveGravity) return
                val rateAboutUp = event.values[0] * up[0] + event.values[1] * up[1] + event.values[2] * up[2]
                // Counter-clockwise about "up" is positive; a heading grows clockwise.
                val degPerS = -Math.toDegrees(rateAboutUp.toDouble()).toFloat()
                onRate?.invoke(degPerS, SystemClock.elapsedRealtime())
            }
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) = Unit
}
