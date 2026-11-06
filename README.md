# rtw89 for macOS

A port of Linux **rtw89** (Realtek Wi-Fi 6/6E/7: RTL8852AE/BE/CE, RTL8851BE,
RTL8922AE) and **btrtl** (their Bluetooth half) to macOS 14–26 on x86_64
OpenCore systems.

**Status: Phase 0, research. There is no code yet and nothing works.** Don't
install anything from this repository yet.

- [Architecture / research report](docs/ARCHITECTURE.md)
- [Risk list](docs/RISKS.md)
- [Status matrix](docs/STATUS.md)

Planned components: `AirportRtw89.kext` (Wi-Fi, native AirPort interface),
`RealtekBTFirmware.kext` (BT patch loader, hands the controller to Apple's
stack via BlueToolFixup), `Rtw89Tool` (debug CLI).

License: GPL-3.0 for now (see `LICENSE`). The choice is still open, see
[ARCHITECTURE §3](docs/ARCHITECTURE.md#3-licensing-a-decision-is-needed-before-code).
Firmware blobs come from linux-firmware under their own redistribution terms.
