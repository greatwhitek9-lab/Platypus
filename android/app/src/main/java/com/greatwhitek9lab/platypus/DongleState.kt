package com.greatwhitek9lab.platypus

/**
 * The fully-automated connection lifecycle. The app walks this chain by
 * itself the moment a dongle is plugged in - there is no "Connect" button
 * anywhere in the UI, only Start/Stop for the *capture file*.
 */
enum class DongleStage {
    WAITING_FOR_DEVICE,   // no Platypus dongle plugged in yet
    REQUESTING_PERMISSION,// Android USB permission dialog in flight
    OPENING,              // opening the USB serial connection
    HANDSHAKING,          // sent PING, waiting for PONG + VERSION
    ARMED,                // handshake complete, advertising scan running
    CAPTURING,            // ARMED + actively writing a pcap file
    ERROR,                // something failed; auto-retries from WAITING
}

data class DongleUiState(
    val stage: DongleStage = DongleStage.WAITING_FOR_DEVICE,
    val firmwareVersion: String? = null,
    val packetCount: Int = 0,
    val lastChannel: Int? = null,
    val lastRssiDbm: Int? = null,
    val capturePath: String? = null,
    val errorMessage: String? = null,
)
