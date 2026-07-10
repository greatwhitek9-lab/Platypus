#!/usr/bin/env python3
import sys
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))

from np_protocols import (  # noqa: E402
    manufacturer_lookup,
    parse_altbeacon,
    parse_eddystone,
    parse_fast_pair,
    parse_ibeacon,
)


class ProtocolTests(unittest.TestCase):
    def test_ibeacon(self):
        sample = "4c00021500112233445566778899aabbccddeeff00010002c5"
        parsed = parse_ibeacon(sample)
        self.assertEqual(parsed["type"], "iBeacon")
        self.assertEqual(parsed["major"], 1)
        self.assertEqual(parsed["minor"], 2)
        self.assertEqual(parsed["tx_power"], -59)

    def test_eddystone_url(self):
        parsed = parse_eddystone("aafe1000036578616d706c6507")
        self.assertEqual(parsed["type"], "Eddystone-URL")
        self.assertEqual(parsed["url"], "https://example.com")

    def test_fast_pair(self):
        parsed = parse_fast_pair("2cfe123456")
        self.assertEqual(parsed["type"], "Fast Pair")
        self.assertEqual(parsed["model_id"], "123456")

    def test_altbeacon(self):
        sample = "5900beac" + ("11" * 20) + "c5" + "00"
        parsed = parse_altbeacon(sample)
        self.assertEqual(parsed["type"], "AltBeacon")
        self.assertEqual(parsed["reference_rssi"], -59)

    def test_manufacturer_lookup(self):
        company_id, name = manufacturer_lookup("4c000215", {0x004C: "Apple, Inc."})
        self.assertEqual(company_id, 0x004C)
        self.assertEqual(name, "Apple, Inc.")


if __name__ == "__main__":
    unittest.main()
