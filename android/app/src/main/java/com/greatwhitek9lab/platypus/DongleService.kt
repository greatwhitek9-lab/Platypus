package com.greatwhitek9lab.platypus

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

private const val CHANNEL_ID = "platypus_capture"
private const val NOTIFICATION_ID = 1

/**
 * Foreground service so the fully-automated connect/arm/capture chain in
 * DongleController keeps running even while the phone is locked or the app
 * is backgrounded - the dongle should not stop sniffing just because the
 * screen turned off.
 */
class DongleService : Service() {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    private lateinit var controller: DongleController

    val state: StateFlow<DongleUiState> get() = controller.state

    inner class LocalBinder : android.os.Binder() {
        fun service(): DongleService = this@DongleService
    }

    private val binder = LocalBinder()

    override fun onCreate() {
        super.onCreate()
        controller = DongleController(applicationContext, scope)
        createNotificationChannel()
        startForegroundWithNotification("Waiting for Platypus dongle...")
        controller.start()

        scope.launch {
            controller.state.collect { s ->
                updateNotification(describeStage(s))
            }
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onDestroy() {
        controller.stop()
        super.onDestroy()
    }

    fun restartCaptureFile() = controller.startNewCaptureFile()

    private fun describeStage(s: DongleUiState): String = when (s.stage) {
        DongleStage.WAITING_FOR_DEVICE -> "Waiting for Platypus dongle..."
        DongleStage.REQUESTING_PERMISSION -> "Requesting USB permission..."
        DongleStage.OPENING -> "Connecting to dongle..."
        DongleStage.HANDSHAKING -> "Starting up dongle..."
        DongleStage.ARMED -> "Armed - waiting for packets"
        DongleStage.CAPTURING -> "Capturing (${s.packetCount} packets)"
        DongleStage.ERROR -> "Error: ${s.errorMessage ?: "unknown"} - retrying"
    }

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID, "Platypus capture status", NotificationManager.IMPORTANCE_LOW
        )
        getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
    }

    private fun buildNotification(text: String): Notification =
        NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Platypus Sniffer")
            .setContentText(text)
            .setSmallIcon(android.R.drawable.stat_sys_download)
            .setOngoing(true)
            .build()

    private fun startForegroundWithNotification(text: String) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            startForeground(
                NOTIFICATION_ID, buildNotification(text),
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
            )
        } else {
            startForeground(NOTIFICATION_ID, buildNotification(text))
        }
    }

    private fun updateNotification(text: String) {
        getSystemService(NotificationManager::class.java)
            .notify(NOTIFICATION_ID, buildNotification(text))
    }
}
