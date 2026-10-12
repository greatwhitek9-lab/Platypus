package com.greatwhitek9lab.platypus

import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Kotlin port of tools/platypus_proto.py - keep the two in sync if the
 * firmware's wire format (firmware/platypus-sniffer/src/proto_framer.c)
 * ever changes.
 *
 * Frame: 0xAA 0x55 | len:u16 LE | type:u8 | payload[len-1] | crc16:u16 LE
 */
object Proto {
    const val MAGIC0: Int = 0xAA
    const val MAGIC1: Int = 0x55

    const val MSG_CAPTURE: Int = 0x01
    const val MSG_PING: Int = 0x02
    const val MSG_PONG: Int = 0x03
    const val MSG_SET_MODE: Int = 0x04
    const val MSG_VERSION: Int = 0x05

    fun crc16CcittFalse(data: ByteArray): Int {
        var crc = 0xFFFF
        for (b in data) {
            crc = crc xor ((b.toInt() and 0xFF) shl 8)
            repeat(8) {
                crc = if (crc and 0x8000 != 0) {
                    ((crc shl 1) xor 0x1021) and 0xFFFF
                } else {
                    (crc shl 1) and 0xFFFF
                }
            }
        }
        return crc
    }

    /**
     * Encode an outgoing command frame (host -> dongle), mirroring the
     * firmware's send_frame() in proto_framer.c. Used by DongleController
     * to drive the automated PING / SET_MODE handshake with no user
     * interaction required.
     */
    fun encodeFrame(type: Int, payload: ByteArray = ByteArray(0)): ByteArray {
        val typeAndPayload = byteArrayOf(type.toByte()) + payload
        val crc = crc16CcittFalse(typeAndPayload)
        val len = typeAndPayload.size

        val out = ByteBuffer.allocate(4 + len + 2).order(ByteOrder.LITTLE_ENDIAN)
        out.put(MAGIC0.toByte())
        out.put(MAGIC1.toByte())
        out.putShort(len.toShort())
        out.put(typeAndPayload)
        out.putShort(crc.toShort())
        return out.array()
    }
}

data class CapturedPacket(
    val accessAddress: Long,
    val channel: Int,
    val rssiDbm: Int,
    val crcOk: Boolean,
    val crc: Long,
    val timestampUs: Long,
    val pdu: ByteArray,
)

/** Fixed part of a MSG_CAPTURE payload, matching proto_framer.c's layout. */
private const val CAPTURE_FIXED_LEN = 4 + 1 + 1 + 1 + 4 + 8 + 1

fun parseCapturePayload(payload: ByteArray): CapturedPacket {
    val buf = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
    val accessAddress = buf.int.toLong() and 0xFFFFFFFFL
    val channel = buf.get().toInt() and 0xFF
    val rssi = buf.get().toInt() // signed
    val crcOk = buf.get().toInt() != 0
    val crc = buf.int.toLong() and 0xFFFFFFFFL
    val ts = buf.long
    val length = buf.get().toInt() and 0xFF
    val pdu = ByteArray(length)
    buf.get(pdu, 0, minOf(length, buf.remaining()))
    return CapturedPacket(accessAddress, channel, rssi, crcOk, crc, ts, pdu)
}

/**
 * Feed raw serial bytes in; get decoded (type, payload) frames out.
 * Mirrors tools/platypus_proto.py's FrameReader.
 */
class FrameReader {
    private val buf = ArrayDeque<Byte>()

    fun feed(data: ByteArray, onFrame: (type: Int, payload: ByteArray) -> Unit) {
        for (b in data) buf.addLast(b)
        while (true) {
            if (!tryExtractOne(onFrame)) break
        }
    }

    private fun tryExtractOne(onFrame: (Int, ByteArray) -> Unit): Boolean {
        while (buf.size >= 2 &&
            !((buf[0].toInt() and 0xFF) == Proto.MAGIC0 &&
                (buf[1].toInt() and 0xFF) == Proto.MAGIC1)
        ) {
            buf.removeFirst()
        }
        if (buf.size < 5) return false

        val length = (buf[2].toInt() and 0xFF) or ((buf[3].toInt() and 0xFF) shl 8)
        val totalNeeded = 2 + 2 + length + 2
        if (buf.size < totalNeeded) return false

        val arr = buf.toByteArray()
        val typeAndPayload = arr.copyOfRange(4, 4 + length)
        val crcRx = (arr[4 + length].toInt() and 0xFF) or
            ((arr[4 + length + 1].toInt() and 0xFF) shl 8)
        val crcCalc = Proto.crc16CcittFalse(typeAndPayload)

        repeat(totalNeeded) { buf.removeFirst() }

        if (crcRx != crcCalc) {
            return tryExtractOne(onFrame) // drop malformed frame, keep resyncing
        }

        val msgType = typeAndPayload[0].toInt() and 0xFF
        val payload = typeAndPayload.copyOfRange(1, typeAndPayload.size)
        onFrame(msgType, payload)
        return true
    }
}
