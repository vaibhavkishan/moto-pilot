package com.vaibhavkishan.motopilot

import android.app.Application
import com.vaibhavkishan.motopilot.network.BleNavClient
import com.vaibhavkishan.motopilot.storage.SettingsRepository

class MotoPilotApplication : Application() {

    lateinit var settingsRepository: SettingsRepository
        private set

    lateinit var navClient: BleNavClient
        private set

    override fun onCreate() {
        super.onCreate()
        settingsRepository = SettingsRepository(this)
        navClient = BleNavClient(this)
    }

    override fun onTerminate() {
        navClient.release()
        super.onTerminate()
    }
}
