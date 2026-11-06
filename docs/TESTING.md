# Testing on real hardware

Test machine (owner's):

| | |
|---|---|
| Board | Gigabyte **B650M D3HP AX rev 1.0/1.2** (AMD AM5) |
| Wi-Fi/BT | **Realtek RTL8852CE** (Wi-Fi 6E, 2.4/5/6 GHz, 160 MHz, BT 5.3) according to [Gigabyte's spec page](https://www.gigabyte.com/Motherboard/B650M-D3HP-AX-rev-10-12/sp). PCI ID expected `10ec:c852` **[verify with the probe]** |
| macOS | 26.7 Tahoe (25G229) |
| SMBIOS | MacBookPro16,1 |
| Boot | OpenCore on the only USB stick. No second boot medium |

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

What it does: attaches to the Realtek PCIe function, reads its PCI IDs and
one read-only chip register (`SYS_CFG1`, which gives the chip cut), and
writes them to the log and the IORegistry. It doesn't power on the chip,
enable DMA, register interrupts or write any device register.

```bash
sudo cp -R AirportRtw89.kext /tmp/ && sudo chown -R root:wheel /tmp/AirportRtw89.kext
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
3. The BT controller's USB VID:PID:

   ```bash
   system_profiler SPUSBDataType SPBluetoothDataType
   ```

4. Whether OCLP root patches are installed (OCLP app → Post-Install Root
   Patch), and your OpenCore version.

If the log says `mmio-unreachable`, the chip didn't answer on BAR2. Send the
log plus `ioreg -l -p IODeviceTree | grep -i -A3 'pci10ec'`.
