package com.greatwhitek9lab.platypus

import java.io.OutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Writes the same LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR pcap format as
 * tools/platypus_sniff.py, so a capture saved on-phone opens in Wireshark
 * identically to one taken on a laptop.
 */
object PcapWriter {
    private const val LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR = 256
    private const val FLAG_DEWHITENED = 1 shl 0
    private const val FLAG_SIGPOWER_VALID = 1 shl 1
    private const val FLAG_REF_AA_VALID = 1 shl 4

    fun writeGlobalHeader(out: OutputStream) {
        val buf = ByteBuffer.allocate(24).order(ByteOrder.LITTLE_ENDIAN)
        buf.putInt(0xA1B2C3D4.toInt())
        buf.putShort(2)
        buf.putShort(4)
        buf.putInt(0)
        buf.putInt(0)
        buf.putInt(65535)
        buf.putInt(LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR)
        out.write(buf.array())
    }

    fun writeRecord(out: OutputStream, pkt: CapturedPacket) {
        val tsSec = (pkt.timestampUs / 1_000_000L)
        val tsUsec = (pkt.timestampUs % 1_000_000L)

        val pseudoHeader = ByteBuffer.allocate(10).order(ByteOrder.LITTLE_ENDIAN)
        pseudoHeader.put(pkt.channel.toByte())
        pseudoHeader.put(pkt.rssiDbm.toByte())
        pseudoHeader.put(0) // noise_power: not measured by this firmware
        pseudoHeader.put(0) // access_address_offenses: not tracked
        pseudoHeader.putInt(pkt.accessAddress.toInt())
        pseudoHeader.putShort(
            (FLAG_DEWHITENED or FLAG_SIGPOWER_VALID or FLAG_REF_AA_VALID).toShort()
        )

        val aaBytes = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN)
            .putInt(pkt.accessAddress.toInt()).array()

        // See tools/platypus_sniff.py for why the trailing CRC is padded
        // zero (firmware doesn't currently recover the raw CRC bytes).
        val body = pseudoHeader.array() + aaBytes + pkt.pdu + byteArrayOf(0, 0, 0)

        val recHdr = ByteBuffer.allocate(16).order(ByteOrder.LITTLE_ENDIAN)
        recHdr.putInt(tsSec.toInt())
        recHdr.putInt(tsUsec.toInt())
        recHdr.putInt(body.size)
        recHdr.putInt(body.size)

        out.write(recHdr.array())
        out.write(body)
    }
}
