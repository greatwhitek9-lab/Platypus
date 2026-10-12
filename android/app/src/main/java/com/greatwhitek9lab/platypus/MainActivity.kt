package com.greatwhitek9lab.platypus

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

/**
 * A thin, mostly-decorative activity: all real work runs in DongleService
 * so capture continues even if this screen isn't open. This just binds to
 * the service and renders its state - there is deliberately no "Connect"
 * button, because connecting is automatic.
 */
class MainActivity : ComponentActivity() {

    private var service: DongleService? = null
    private val boundState = MutableStateFlow<StateFlow<DongleUiState>?>(null)

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            val svc = (binder as DongleService.LocalBinder).service()
            service = svc
            boundState.value = svc.state
        }

        override fun onServiceDisconnected(name: ComponentName) {
            service = null
            boundState.value = null
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val intent = Intent(this, DongleService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(intent)
        } else {
            startService(intent)
        }
        bindService(intent, connection, Context.BIND_AUTO_CREATE)

        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    val flow by boundState.collectAsState()
                    val uiState by (flow ?: remember { MutableStateFlow(DongleUiState()) })
                        .collectAsState()
                    DashboardScreen(
                        state = uiState,
                        onNewCaptureFile = { service?.restartCaptureFile() },
                    )
                }
            }
        }
    }

    override fun onDestroy() {
        runCatching { unbindService(connection) }
        super.onDestroy()
    }
}

@Composable
fun DashboardScreen(state: DongleUiState, onNewCaptureFile: () -> Unit) {
    Scaffold(
        topBar = {
            TopAppBar(title = { Text("Platypus Sniffer") })
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            StatusCard(state)
            StatsCard(state)
            Spacer(Modifier.height(8.dp))
            Button(onClick = onNewCaptureFile, modifier = Modifier.fillMaxWidth()) {
                Text("Start new capture file")
            }
            state.capturePath?.let {
                Text(
                    "Saving to: $it",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }
    }
}

@Composable
private fun StatusCard(state: DongleUiState) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = statusColor(state.stage)),
    ) {
        Row(
            modifier = Modifier.padding(20.dp).fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            if (state.stage == DongleStage.OPENING || state.stage == DongleStage.HANDSHAKING) {
                CircularProgressIndicator(modifier = Modifier.height(28.dp))
            } else {
                Icon(Icons.Filled.Bluetooth, contentDescription = null)
            }
            Column {
                Text(statusHeadline(state.stage), style = MaterialTheme.typography.titleMedium)
                Text(statusSubtext(state), style = MaterialTheme.typography.bodySmall)
            }
        }
    }
}

@Composable
private fun StatsCard(state: DongleUiState) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            StatRow("Packets captured", state.packetCount.toString())
            StatRow("Last channel", state.lastChannel?.toString() ?: "-")
            StatRow("Last RSSI", state.lastRssiDbm?.let { "$it dBm" } ?: "-")
            StatRow("Firmware", state.firmwareVersion ?: "-")
        }
    }
}

@Composable
private fun StatRow(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(label, style = MaterialTheme.typography.bodyMedium)
        Text(value, style = MaterialTheme.typography.bodyMedium)
    }
}

private fun statusHeadline(stage: DongleStage): String = when (stage) {
    DongleStage.WAITING_FOR_DEVICE -> "Waiting for dongle"
    DongleStage.REQUESTING_PERMISSION -> "Requesting permission"
    DongleStage.OPENING -> "Connecting"
    DongleStage.HANDSHAKING -> "Starting up"
    DongleStage.ARMED -> "Armed"
    DongleStage.CAPTURING -> "Capturing"
    DongleStage.ERROR -> "Error - retrying"
}

private fun statusSubtext(state: DongleUiState): String = when (state.stage) {
    DongleStage.WAITING_FOR_DEVICE -> "Plug in the Platypus dongle via USB-OTG"
    DongleStage.REQUESTING_PERMISSION -> "Accept the USB permission prompt"
    DongleStage.OPENING -> "Opening serial connection..."
    DongleStage.HANDSHAKING -> "Pinging dongle, arming scan..."
    DongleStage.ARMED -> "Listening for BLE advertising traffic"
    DongleStage.CAPTURING -> "Writing packets to pcap"
    DongleStage.ERROR -> state.errorMessage ?: "Unknown error"
}

@Composable
private fun statusColor(stage: DongleStage) = when (stage) {
    DongleStage.CAPTURING, DongleStage.ARMED ->
        MaterialTheme.colorScheme.primaryContainer
    DongleStage.ERROR -> MaterialTheme.colorScheme.errorContainer
    else -> MaterialTheme.colorScheme.surfaceVariant
}
