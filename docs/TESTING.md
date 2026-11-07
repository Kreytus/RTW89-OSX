# Testing on real hardware

Test machine (owner's):

| | |
|---|---|
| Board | Gigabyte **B650M D3HP AX rev 1.0/1.2** (AMD AM5) |
| Wi-Fi/BT | **Realtek RTL8852CE** (Wi-Fi 6E, 2.4/5/6 GHz, 160 MHz, BT 5.3) according to [Gigabyte's spec page](https://www.gigabyte.com/Motherboard/B650M-D3HP-AX-rev-10-12/sp). PCI ID expected `10ec:c852` **[verify with the probe]** |
| macOS | 26.7 Tahoe (25G229) |
| SMBIOS | MacBookPro16,1 |
| Boot | OpenCore on the only USB stick. No second boot medium |
| SIP | Fully disabled (`csrutil status`, owner report) |
| USB map | USBToolBox (`USBToolBox.kext` + `UTBMap.kext`) |
| OCLP | **No root patches installed.** IO80211/IOSkywalkFamily are stock 26.7 |
| BT USB device | **`0bda:0852`** "Bluetooth Radio" (owner's `ioreg -p IOUSB`). The port is already in the USBToolBox map. Not listed by ID in btusb, but matched there by the generic Realtek rule (`0x0bda`, class E0/01/01). btrtl picks the firmware at runtime from `lmp_subver`/`hci_rev` |
| BT as macOS sees it | Only the placeholder bluetoothd shows when it accepts **no** controller (`BCM_4350C2`, UART, address NULL), as expected with no Realtek loader yet |

## Rule 1: never inject development kexts through OpenCore

The only boot medium is the OpenCore USB. A kext injected from
`EFI/OC/Kexts` that panics at boot would leave the machine unbootable until
the stick is fixed from another computer. So, until a build has survived many
runtime loads:

- **Load development kexts at runtime with `kmutil`, after boot.** If a kext
  panics, the machine reboots into a normal system without it.
- Add a kext to `config.plist` only after it's been marked stable in
  `STATUS.md`.
- Before any OpenCore change, **make a second boot path**: copy the USB's
  `EFI` folder to the internal disk's EFI partition (or a second stick), and
  check that you can boot from it. Then the USB becomes the rescue copy.

## One-time setup

1. Check SIP. Loading an unsigned kext needs at least
   `CSR_ALLOW_UNTRUSTED_KEXTS` (0x1) and `CSR_ALLOW_UNAPPROVED_KEXTS` (0x200).
   Many hackintosh configs already use a wider value.

   ```bash
   csrutil status
   ```

   ```bash
   nvram 7C436110-AB2A-4BBB-A880-FE41995C9F82:csr-active-config
   ```

   **[verify on 26.7]** whether Tahoe still allows `kmutil load` of an
   unsigned kext at runtime with those bits set, or whether it insists on the
   "System Extension Blocked" approval and a reboot. Report what happens.

2. Get the build: the GitHub Actions run for the branch → artifact
   `AirportRtw89-<sha>` → unzip. Use the **Debug** kext.

## Phase 1a: read-only probe

Your current network connection is not affected. The kext matches only the
Realtek Wi-Fi PCI IDs (`10ec:c852` and the other rtw89 IDs), which have no
macOS driver today. It does not match the onboard Ethernet controller or any
USB adapter, and `kmutil unload -b org.rtw89osx.AirportRtw89` unloads only
this kext. Worst case (a panic), the machine reboots and comes back up
without it.

What it does: attaches to the Realtek PCIe function, reads its PCI IDs and
one read-only chip register (`SYS_CFG1`, which gives the chip cut), and
writes them to the log and the IORegistry. It doesn't power on the chip,
enable DMA, register interrupts or write any device register.

The artifact zip has `Debug/AirportRtw89.kext` and
`Release/AirportRtw89.kext` inside. Safari may unzip it on its own into a
folder named `AirportRtw89-<sha>`. These commands work either way:

```bash
cd ~/Downloads && for z in AirportRtw89-*.zip; do [ -e "$z" ] && unzip -o "$z" -d "${z%.zip}"; done
```

```bash
K=$(find ~/Downloads -type d -name AirportRtw89.kext -path '*Debug*' | head -1); echo "$K"
```

```bash
sudo rm -rf /tmp/AirportRtw89.kext; sudo cp -R "$K" /tmp/ && sudo chown -R root:wheel /tmp/AirportRtw89.kext && sudo xattr -cr /tmp/AirportRtw89.kext
```

```bash
sudo kmutil load -p /tmp/AirportRtw89.kext
```

```bash
log show --last 5m --predicate 'eventMessage CONTAINS "rtw89:"'
```

```bash
ioreg -rc Rtw89PCIProbe -l | grep rtw89
```

```bash
sudo kmutil unload -b org.rtw89osx.AirportRtw89
```

### Phase 1a results so far

| Date | Build | Result |
|---|---|---|
| 2026-11-07 | 0.0.1 | Kext loaded and matched. `IOPCIDevice::open()` failed: **another client already has the Wi-Fi function open**. No MMIO access was attempted. Build 0.0.2 logs the PCI IDs before opening, plus every client attached to the device, to identify the owner |

Expected log lines:

```
rtw89: PCI 10ec:c852 subsys xxxx:xxxx rev xx (RTL8852CE) at b:d.f
rtw89: BAR2 phys 0x... len 0x..., SYS_CFG1 = 0x....
rtw89: RTL8852CE, chip cut X (cv=N)
rtw89: stopping                       <- after unload
```

What to send back:

1. All `rtw89:` log lines, from load and from unload.
2. Whether load and unload both worked without a panic, and whether loading
   needed approval or a reboot.
3. The BT controller's USB VID:PID. `SPBluetoothDataType` doesn't show it
   (it only lists controllers bluetoothd accepted), so list raw USB devices.
   Realtek's vendor ID is `0x0bda` (3034); OEM IDs such as `0x13d3`,
   `0x0489`, `0x04ca`, `0x04c5` and `0x0cb8` are also common for 8852CE:

   ```bash
   ioreg -p IOUSB -l -w0 | grep -E '"USB Product Name"|"idVendor"|"idProduct"'
   ```

   If nothing Realtek-like shows up, the internal USB port the BT module sits
   on is probably not in your USB port map. The BT half can't be tested
   until that port is mapped. Wi-Fi isn't affected. With USBToolBox: run the
   USBToolBox tool, discover ports, find the port the Realtek BT device shows
   up on (an internal USB 2.0 header port), include it with type
   **Internal (255)**, rebuild `UTBMap.kext` and replace the old one in
   `EFI/OC/Kexts`. Back up the EFI first (see Rule 1): this is a boot-time
   change.

4. Your OpenCore version (OCLP: answered, none installed).

If the log says `mmio-unreachable`, the chip didn't answer on BAR2. Send the
log plus `ioreg -l -p IODeviceTree | grep -i -A3 'pci10ec'`.
