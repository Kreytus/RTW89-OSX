# rtw89 for macOS

A port of Linux **rtw89** (Realtek Wi-Fi 6/6E/7: RTL8852AE/BE/CE, RTL8851BE,
RTL8922AE) and **btrtl** (their Bluetooth half) to macOS 14–26 on x86_64
OpenCore systems.

**Status: Phase 1a. The only code is a read-only PCIe probe, and it hasn't
been tested yet.** Nothing works yet. Don't put anything from this repository
into your OpenCore config.

- [Hardware testing guide](docs/TESTING.md)
- [Architecture / research report](docs/ARCHITECTURE.md)
- [Risk list](docs/RISKS.md)
- [Status matrix](docs/STATUS.md)

Planned components: `AirportRtw89.kext` (Wi-Fi, native AirPort interface),
`RealtekBTFirmware.kext` (BT patch loader, hands the controller to Apple's
stack via BlueToolFixup), `Rtw89Tool` (debug CLI).

License: GPL-3.0 (see `LICENSE`). The mac80211 compatibility layer is a
clean-room reimplementation, see
[ARCHITECTURE §3](docs/ARCHITECTURE.md#3-licensing-decided-gpl-30-clean-room-shim).

Build (macOS, Xcode or Command Line Tools):

```bash
make sdk && make
```

Firmware blobs come from linux-firmware under their own redistribution terms.
