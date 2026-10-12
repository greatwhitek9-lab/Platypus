package com.greatwhitek9lab.platypus

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.driver.UsbSerialProber
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.io.File
import java.io.FileOutputStream

private const val USB_PERMISSION_ACTION = "com.greatwhitek9lab.platypus.USB_PERMISSION"
private const val HANDSHAKE_TIMEOUT_MS = 3000L
private const val RETRY_DELAY_MS = 2000L

/**
 * Owns the entire dongle lifecycle end to end, with zero manual steps:
 *
 *   plug in -> request USB permission -> open serial port -> send PING
 *   -> on PONG, send SET_MODE (arm advertising scan) -> start writing a
 *   pcap file -> on unplug/error, go back to waiting and retry
 *   automatically next time a device shows up.
 *
 * The only thing a user can do from the UI is stop/restart the *capture
 * file* (closeCaptureFile / startNewCaptureFile) - the connection and
 * command handshake with the dongle itself is never user-driven.
 */
class DongleController(private val context: Context, private val scope: CoroutineScope) {

    private val _state = MutableStateFlow(DongleUiState())
    val state: StateFlow<DongleUiState> = _state

    private val usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager
    private var serialPort: UsbSerialPort? = null
    private var pcapStream: FileOutputStream? = null
    private var readJob: Job? = null
    private var handshakeJob: Job? = null
    private val frameReader = FrameReader()

    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) {
            when (intent.action) {
                USB_PERMISSION_ACTION -> {
                    val granted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)
                    val device: UsbDevice? = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
                    if (granted && device != null) {
                        openAndHandshake(device)
                    } else {
                        _state.update {
                            it.copy(stage = DongleStage.ERROR, errorMessage = "USB permission denied")
                        }
                        scheduleRetry()
                    }
                }
                UsbManager.ACTION_USB_DEVICE_ATTACHED,
                UsbManager.ACTION_USB_DEVICE_DETACHED -> findAndConnect()
            }
        }
    }

    /** Call once from the service/activity's onCreate. Starts the automatic loop. */
    fun start() {
        val filter = IntentFilter().apply {
            addAction(USB_PERMISSION_ACTION)
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }
        context.registerReceiver(usbReceiver, filter)
        findAndConnect()
    }

    fun stop() {
        handshakeJob?.cancel()
        readJob?.cancel()
        runCatching { context.unregisterReceiver(usbReceiver) }
        closeCaptureFile()
        serialPort?.close()
        serialPort = null
    }

    // --- automatic connect chain -------------------------------------------------

    private fun findAndConnect() {
        val drivers = UsbSerialProber.getDefaultProber().findAllDrivers(usbManager)
        if (drivers.isEmpty()) {
            _state.update { DongleUiState(stage = DongleStage.WAITING_FOR_DEVICE) }
            return
        }
        val device = drivers[0].device

        if (usbManager.hasPermission(device)) {
            openAndHandshake(device)
            return
        }

        _state.update { it.copy(stage = DongleStage.REQUESTING_PERMISSION) }
        val permissionIntent = PendingIntent.getBroadcast(
            context, 0, Intent(USB_PERMISSION_ACTION), PendingIntent.FLAG_MUTABLE
        )
        usbManager.requestPermission(device, permissionIntent)
    }

    private fun openAndHandshake(device: UsbDevice) {
        _state.update { it.copy(stage = DongleStage.OPENING, errorMessage = null) }

        val driver = UsbSerialProber.getDefaultProber().probeDevice(device)
        val connection = driver?.let { usbManager.openDevice(device) }
        if (driver == null || connection == null) {
            _state.update {
                it.copy(stage = DongleStage.ERROR, errorMessage = "Could not open dongle")
            }
            scheduleRetry()
            return
        }

        val port = driver.ports[0]
        runCatching {
            port.open(connection)
            port.setParameters(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
        }.onFailure {
            _state.update { s -> s.copy(stage = DongleStage.ERROR, errorMessage = it.message) }
            scheduleRetry()
            return
        }
        serialPort = port

        _state.update { it.copy(stage = DongleStage.HANDSHAKING) }
        startReadLoop(port)
        runAutomatedHandshake(port)
    }

    /**
     * The "fully automated commands" sequence: no user ever taps anything
     * to make this happen. PING confirms the link is alive; SET_MODE arms
     * promiscuous advertising-channel scanning on the firmware side
     * (see channel_hop_start_adv_scan() in the firmware). If no PONG shows
     * up in time, we assume the port is stale and retry the whole chain.
     */
    private fun runAutomatedHandshake(port: UsbSerialPort) {
        handshakeJob?.cancel()
        handshakeJob = scope.launch(Dispatchers.IO) {
            runCatching {
                port.write(Proto.encodeFrame(Proto.MSG_PING), 500)
            }
            kotlinx.coroutines.delay(HANDSHAKE_TIMEOUT_MS)
            if (_state.value.stage == DongleStage.HANDSHAKING) {
                // No PONG/VERSION observed by the read loop in time.
                _state.update {
                    it.copy(stage = DongleStage.ERROR, errorMessage = "No response from dongle")
                }
                scheduleRetry()
            }
        }
    }

    private fun armAdvertisingScanAndCapture(port: UsbSerialPort) {
        runCatching { port.write(Proto.encodeFrame(Proto.MSG_SET_MODE), 500) }
        startNewCaptureFile()
        _state.update { it.copy(stage = DongleStage.CAPTURING) }
    }

    private fun scheduleRetry() {
        readJob?.cancel()
        serialPort?.let { runCatching { it.close() } }
        serialPort = null
        scope.launch {
            kotlinx.coroutines.delay(RETRY_DELAY_MS)
            findAndConnect()
        }
    }

    // --- read loop / capture file -------------------------------------------------

    private fun startReadLoop(port: UsbSerialPort) {
        readJob?.cancel()
        readJob = scope.launch(Dispatchers.IO) {
            val buf = ByteArray(4096)
            while (true) {
                val len = runCatching { port.read(buf, 500) }.getOrElse {
                    _state.update { s -> s.copy(stage = DongleStage.ERROR, errorMessage = it.message) }
                    scheduleRetry()
                    return@launch
                }
                if (len <= 0) continue
                frameReader.feed(buf.copyOf(len)) { type, payload ->
                    handleFrame(port, type, payload)
                }
            }
        }
    }

    private fun handleFrame(port: UsbSerialPort, type: Int, payload: ByteArray) {
        when (type) {
            Proto.MSG_VERSION -> {
                val version = String(payload)
                _state.update { it.copy(firmwareVersion = version) }
            }
            Proto.MSG_PONG -> {
                if (_state.value.stage == DongleStage.HANDSHAKING) {
                    armAdvertisingScanAndCapture(port)
                }
            }
            Proto.MSG_CAPTURE -> {
                val pkt = parseCapturePayload(payload)
                pcapStream?.let { PcapWriter.writeRecord(it, pkt) }
                _state.update {
                    it.copy(
                        packetCount = it.packetCount + 1,
                        lastChannel = pkt.channel,
                        lastRssiDbm = pkt.rssiDbm,
                    )
                }
            }
        }
    }

    // --- capture file management (the only user-facing control) -------------------

    fun startNewCaptureFile() {
        closeCaptureFile()
        val dir = context.getExternalFilesDir(null)
        val file = File(dir, "platypus_${System.currentTimeMillis()}.pcap")
        val out = FileOutputStream(file)
        PcapWriter.writeGlobalHeader(out)
        pcapStream = out
        _state.update { it.copy(capturePath = file.absolutePath, packetCount = 0) }
    }

    fun closeCaptureFile() {
        pcapStream?.let { runCatching { it.close() } }
        pcapStream = null
    }
}
