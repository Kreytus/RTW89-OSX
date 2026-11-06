# Status

Legend: **working** (tested on real hardware), **partial**, **untested**
(code exists, not run on hardware), **not started**.

Current phase: **1a, read-only PCIe probe** (code exists, untested).
Test machine: B650M D3HP AX, RTL8852CE, macOS 26.7 (25G229).

## Feature × macOS (RTL8852CE, primary test chip)

| Feature | 14 Sonoma | 15 Sequoia | 26 Tahoe |
|---|---|---|---|
| PCIe probe + chip ID (Phase 1a) | not started | not started | untested |
| Power-on / FW download / MAC read (Phase 1b) | not started | not started | not started |
| Native Wi-Fi menu + Settings pane | not started | not started | not started |
| Scan 2.4 / 5 / 6 GHz | not started | not started | not started |
| Open / WPA2-PSK / WPA3-SAE | not started | not started | not started |
| HE rates, 80/160 MHz | not started | not started | not started |
| Sleep/wake, reconnect, roaming | not started | not started | not started |
| BT firmware upload | not started | not started | not started |
| BT pairing / HID / BLE | not started | not started | not started |
| BT A2DP / HFP | not started | not started | not started |
| Coex (A2DP + Wi-Fi throughput) | not started | not started | not started |
| AirDrop / AWDL | research-gated | research-gated | research-gated |
| Handoff / Universal Clipboard | research-gated | research-gated | research-gated |

## Other chips

RTL8852BE, RTL8852BT, RTL8851BE, RTL8852AE, RTL8922AE: not started (planned
after 8852CE; see ARCHITECTURE §2).
