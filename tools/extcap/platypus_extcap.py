#!/usr/bin/env python3
"""
platypus_extcap.py - Wireshark extcap plugin for the Platypus Sniffer.

Install by symlinking/copying into Wireshark's extcap directory, e.g.:
    ln -s $(pwd)/platypus_extcap.py \
        ~/.local/lib/wireshark/extcap/platypus_extcap.py
    chmod +x ~/.local/lib/wireshark/extcap/platypus_extcap.py

After that, "Platypus BLE Sniffer" shows up as a normal capture interface in
Wireshark's interface list - this mirrors how Nordic's nRF Sniffer and other
BLE/Zigbee USB sniffers integrate, so the workflow is: pick the interface,
hit the blue shark fin, watch BLE traffic live.

Wireshark calls this script multiple times with different flag
combinations (--extcap-interfaces, --extcap-dlts, --extcap-config,
--capture ...) - see the extcap spec:
https://www.wireshark.org/docs/wsdg_html_chunked/ChCaptureExtcap.html
"""
from __future__ import annotations

import argparse
import glob
import sys

sys.path.insert(0, __file__.rsplit("/tools/", 1)[0] + "/tools")

from platypus_proto import FrameReader, MSG_CAPTURE, parse_capture_payload  # noqa: E402
from platypus_sniff import PCAP_GLOBAL_HDR, build_pcap_record  # noqa: E402

INTERFACE = "platypus0"
DLT = 256  # LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR


def list_interfaces() -> None:
    print(f"extcap {{version=0.1.0}}{{help=https://github.com/greatwhitek9-lab/Platypus}}")
    print(f"interface {{value={INTERFACE}}}{{display=Platypus BLE Sniffer}}")


def list_dlts() -> None:
    print(f"dlt {{number={DLT}}}{{name=BLUETOOTH_LE_LL_WITH_PHDR}}"
          f"{{display=Bluetooth LE Link Layer with PHDR}}")


def show_config() -> None:
    ports = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
    default_port = ports[0] if ports else "/dev/ttyACM0"
    print("arg {number=0}{call=--port}{display=Serial port}"
          "{type=string}{default=%s}{tooltip=Platypus dongle serial port}"
          % default_port)


def do_capture(fifo_path: str, port: str, baud: int) -> int:
    import serial  # imported late so --extcap-interfaces works with no deps

    ser = serial.Serial(port, baud, timeout=0.2)
    reader = FrameReader()

    with open(fifo_path, "wb") as fifo:
        fifo.write(PCAP_GLOBAL_HDR)
        fifo.flush()
        try:
            while True:
                data = ser.read(4096)
                if not data:
                    continue
                for msg_type, payload in reader.feed(data):
                    if msg_type == MSG_CAPTURE:
                        pkt = parse_capture_payload(payload)
                        build_pcap_record(pkt, fifo)
                        fifo.flush()
        except (KeyboardInterrupt, BrokenPipeError):
            pass
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("--capture", action="store_true")
    ap.add_argument("--extcap-interfaces", action="store_true")
    ap.add_argument("--extcap-dlts", action="store_true")
    ap.add_argument("--extcap-config", action="store_true")
    ap.add_argument("--extcap-interface", default=INTERFACE)
    ap.add_argument("--fifo")
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    # Wireshark also passes --extcap-version, --extcap-reload-option, etc.
    args, _unknown = ap.parse_known_args()

    if args.extcap_interfaces:
        list_interfaces()
        return 0
    if args.extcap_dlts:
        list_dlts()
        return 0
    if args.extcap_config:
        show_config()
        return 0
    if args.capture:
        if not args.fifo:
            print("error: --capture requires --fifo", file=sys.stderr)
            return 1
        return do_capture(args.fifo, args.port, args.baud)

    list_interfaces()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
