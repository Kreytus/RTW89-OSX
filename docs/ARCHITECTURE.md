# Architecture: rtw89 for macOS (Phase 0 research report)

Status: **Phase 0, research only. No code exists yet and nothing has been
tested on hardware.** Everything below comes from reading source. Claims that
need hardware or disassembly to confirm are marked **[verify]**.

Risk list: [`RISKS.md`](RISKS.md). Feature matrix: [`STATUS.md`](STATUS.md).

---

## 1. Sources studied

| Source | Revision | License (SPDX / file header) | How we use it |
|---|---|---|---|
| Linux `drivers/net/wireless/realtek/rtw89/` | torvalds/linux `f66469d9905b` (2026-11-05) | `GPL-2.0 OR BSD-3-Clause` (every file) | Port base for Wi-Fi |
| Linux `net/mac80211/`, `net/wireless/`, `include/net/mac80211.h`, `include/net/cfg80211.h` | same | **`GPL-2.0-only`** (58 files), `GPL-2.0` (19), a few ISC/BSD-2 | API reference only (see §3) |
| Linux `drivers/bluetooth/btrtl.c`, `btusb_main.c` | same | `GPL-2.0-or-later` | Port base for BT |
| OpenIntelWireless/itlwm (+AirportItlwm) | `53c51c2` (2024-06-09) | GPL-2.0-or-later | IO80211 glue reference |
| OpenIntelWireless/IntelBluetoothFirmware | 2026-01-15 | GPL-3.0 | USB BT attach/upload reference |
| acidanthera/BrcmPatchRAM (BlueToolFixup) | 2026-03-20 | GPL-2.0 | bluetoothd patching reference |
| seemoo-lab/owl | 2026-10-12 | GPL-3.0 | AWDL reference (Phase 5) |

Note: `git.kernel.org` (wireless-next, bluetooth-next) was blocked from the
research environment (proxy 403), so this report reads **mainline**. The port
itself must track wireless-next and bluetooth-next; the delta to mainline is
normally a few weeks of patches. Pin the exact upstream commit in
`linux_compat/UPSTREAM` once code exists.

Raw inventory of every mac80211/cfg80211 call site:
[`research/rtw89-mac80211-calls.md`](research/rtw89-mac80211-calls.md).

---

## 2. Supported chips (planned, from upstream ID tables)

| Chip | Wi-Fi PCI ID (10ec:) | FW file(s) | BT USB IDs (btusb_main.c) | BT FW |
|---|---|---|---|---|
| **RTL8852BE** (primary) | b852, b85b | `rtw89/rtw8852b_fw.bin`, `_fw-1.bin`, `_fw-2.bin` (`FW_FORMAT_MAX 2`) | 0bda:4853, 0bda:887b, 0bda:b85b, 0cb8:c559, 13d3:3570/3571/3572/3591, 0489:e123/e125 | `rtl_bt/rtl8852bu_fw.bin` + `rtl8852bu_config.bin` |
| RTL8852BE-VT (8852BT) | b520 | `rtw89/rtw8852bt_fw.bin` (`FW_FORMAT_MAX 0`) | 0bda:8520, 0489:e12f, 13d3:3618/3619 | `rtl_bt/rtl8852btu_fw.bin` |
| RTL8852AE | 8852, a85a | `rtw89/rtw8852a_fw[-1].bin` | 0bda:2852/c852/385a/4852, 04c5:165c, 04ca:4006, 0cb8:c549 | `rtl_bt/rtl8852au_fw.bin` |
| RTL8852CE | c852 | `rtw89/rtw8852c_fw[-1,-2].bin` | 04ca:4007, 04c5:1675, 0cb8:c558, 13d3:3586/3587/3592/3612, 0489:e122 | `rtl_bt/rtl8852cu_fw.bin` |
| RTL8851BE | b851 | `rtw89/rtw8851b_fw[-1].bin` | 0bda:b850, 13d3:3600/3601, 0489:e112 | `rtl_bt/rtl8851bu_fw.bin` |
| RTL8922AE (Wi-Fi 7) | 8922, 892b | `rtw89/rtw8922a_fw[-1..-4].bin` | 0bda:8922/d922/d923, 13d3:3616/3617, 0489:e130 | `rtl_bt/rtl8922au_fw.bin` |

Upstream also has RTL8922DE (10ec:892d/882d/895d) and USB-only variants
(`rtw88xxu.c`). They are **out of scope** until the PCIe chips work.
RTL8922AE needs MLO/EHT, which IO80211 has no known public interface for, so
it would run as an HE/EHT single-link station at best **[verify]**.

Order: 8852BE first, then 8852BT/8851BE (same `rtw8852b_common.c` family),
then 8852AE/8852CE, then 8922AE last.

---

## 3. Licensing: a decision is needed before code

The repository `LICENSE` is **GPL-3.0**. What that means for each input:

| Input | License | Can go into a GPL-3.0 repo? |
|---|---|---|
| rtw89 driver | GPL-2.0 OR BSD-3-Clause | Yes (take it under BSD-3-Clause) |
| btrtl.c / btusb | GPL-2.0-or-later | Yes |
| itlwm glue | GPL-2.0-or-later | Yes |
| IntelBluetoothFirmware, OWL | GPL-3.0 | Yes |
| BrcmPatchRAM | GPL-2.0 (check per-file "or later") | Only as a reference unless "or later" |
| **mac80211 / cfg80211 code and headers** | **GPL-2.0-only** | **No.** GPL-2.0-only code can't be combined with GPL-3.0 code |

The point that matters: rtw89 is written against `mac80211.h`, and mac80211
supplies most of the 802.11 behaviour the driver relies on (see §4.3). Two
consistent choices:

- **Option L1, keep GPL-3.0 (recommended):** write `linux_compat/` as a
  clean-room reimplementation of the subset of the mac80211/cfg80211 API that
  rtw89 uses (struct layouts and function signatures, which are an interface)
  plus our own 802.11 state machines. We copy no mac80211 code. More work, but
  the license stays clean and the shim stays a shim.
- **Option L2, relicense to GPL-2.0-only:** we could then port the parts of
  `net/mac80211` we need (RX reorder, A-MSDU, agg-tx, MLME) almost verbatim.
  That gets us to correct behaviour faster, but the GPL-3.0 references
  (IntelBluetoothFirmware, OWL) could then only be read, never copied. The
  "shim" would also no longer be thin: `net/mac80211` is ~82 k lines and
  `net/wireless` is ~50 k.

Firmware: rtw89 and rtl_bt blobs come from linux-firmware. They are
redistributable under the Realtek license that `WHENCE` names. **[verify]**
Check `WHENCE` at the pinned linux-firmware commit and ship
`LICENCE.rtlwifi_firmware.txt` (or whatever file it names) inside each kext's
`Resources/`. The firmware is not GPL and is not linked into the kext.

---

## 4. Wi-Fi: what rtw89 needs from mac80211

### 4.1 Callbacks rtw89 implements (`struct ieee80211_ops rtw89_ops`, mac80211.c)

51 ops: `tx, wake_tx_queue, start, stop, config, add_interface,
change_interface, remove_interface, configure_filter, vif_cfg_changed,
link_info_changed, start_ap, stop_ap, set_tim, conf_tx, sta_state, set_key,
ampdu_action, get_survey, set_rts_threshold, sta_statistics, flush,
set_bitrate_mask, set_antenna, get_antenna, sw_scan_start, sw_scan_complete,
reconfig_complete, hw_scan, cancel_hw_scan, add/remove/change_chanctx,
assign/unassign/switch_vif_chanctx, channel_switch_beacon, remain_on_channel,
cancel_remain_on_channel, set_sar_specs, link_sta_rc_update, set_tid_config,
can_activate_links, change_vif_links, change_sta_links, suspend, resume,
set_wakeup, set_rekey_data, rfkill_poll`.

For a macOS **station-only** first release, our glue must drive this subset:
`start, stop, config, add/remove_interface, configure_filter,
vif_cfg_changed, link_info_changed, conf_tx, sta_state, set_key,
ampdu_action, wake_tx_queue/tx, hw_scan/cancel_hw_scan (or sw_scan_*),
chanctx ops (add/assign/unassign/remove/change), flush, sta_statistics,
suspend/resume/set_wakeup, set_rekey_data`. AP, P2P, MLO, SAR and TID config
ops are compiled but not called at first.

### 4.2 Functions rtw89 calls (103 distinct; full table in research/)

Grouped by what the shim must provide:

| Group | Symbols (call sites) | Notes |
|---|---|---|
| wiphy work/locking | `wiphy_lock/unlock`, `wiphy_work_*`, `wiphy_delayed_work_*` (≈95 sites) | Map to one serialised `IOWorkLoop` + `IOCommandGate`; `wiphy_lock` = gate held. **The single biggest piece of glue.** |
| Registration | `ieee80211_alloc_hw/free_hw/register_hw/unregister_hw`, `ieee80211_hw_set` (18), `wiphy_ext_feature_set`, `regulatory_hint` | Fill a static capability struct; no real cfg80211 |
| Iteration | `ieee80211_iterate_stations_atomic` (19), `ieee80211_iterate_active_interfaces_atomic`, `ieee80211_iter_keys`, `ieee80211_find_sta` | Simple lists, under RCU semantics (see below) |
| TX queues | `ieee80211_tx_dequeue_ni`, `next_txq`, `return_txq`, `schedule_txq`, `txq_schedule_start/end`, `txq_get_depth`, `stop/wake_queues`, `purge_tx_queue`, `free_txskb` | We must implement per-STA/TID TXQs (fq_codel-like). This decides throughput |
| TX status | `ieee80211_tx_status_irqsafe/ni`, `tx_info_clear_status` | Feed status into BA/retry accounting and Apple stats |
| RX | `ieee80211_rx_napi` (1) | **One call, but behind it sits all of mac80211's RX path** (§4.3) |
| BA sessions | `ieee80211_start/stop_tx_ba_session`, `stop_tx_ba_cb_irqsafe`, `sta_recalc_aggregates` | Requires ADDBA/DELBA frame handling in our MLME |
| Templates | `ieee80211_probereq_get`, `nullfunc_get`, `pspoll_get`, `proberesp_get`, `beacon_get_tim` | Frame builders we write |
| MLME events | `ieee80211_connection_loss`, `beacon_loss`, `cqm_rssi_notify`, `scan_completed`, `ready_on_channel`, `remain_on_channel_expired`, `restart_hw`, `csa_finish` | Become Apple80211 events (link down, scan done, RSSI) |
| Keys/WoWLAN | `ieee80211_get/set_key_rx_seq`, `gtk_rekey_add/notify`, `report_wowlan_wakeup` | GTK rekey offload during sleep |
| Helpers | `ieee80211_hdrlen`, `is_*`/`has_*` frame tests, `channel_to_frequency`, `tu_to_usec`, `cfg80211_find_ie/elem`, `cfg80211_calculate_bitrate`, `cfg80211_chandef_create`, `cfg80211_channel_is_psc`, `cfg80211_bss_iter` | Pure functions, easy to write clean-room |
| MLO | `ieee80211_vif_is_mld` (23), `vif_usable_links`, `set_active_links` | Stub: always non-MLD |
| Debugfs/LED/rfkill | `wiphy_locked_debugfs_*`, LED, `wiphy_rfkill_*` | Stub; expose via `Rtw89Tool` user client instead |

rtw89 also touches about **68 distinct `struct ieee80211_*`/`cfg80211_*` types**
(`ieee80211_vif`, `_sta`, `_link_sta`, `_bss_conf`, `_chanctx_conf`,
`_key_conf`, `_tx_info`, `_rx_status`, `_txq`, `_hw`, HE/EHT capability
structs, etc.). The shim must define these with the **field names** rtw89
uses. Binary layout compatibility with Linux is not needed.

Kernel APIs, in order of frequency: bitfield helpers (`BIT`, `GENMASK`,
`FIELD_PREP/GET`, `le32_encode_bits`…, >3 000 sites; header-only, trivial),
`dev_kfree_skb_any`/`skb_*` (~250), `udelay/fsleep/mdelay`,
`read_poll_timeout[_atomic]` (178), `rcu_read_lock/unlock` (135), bitops,
lists, `spin_lock_bh/irqsave`, `ewma_*`, `dma_map_single/alloc_coherent`,
`pci_*`, `request_firmware`, NAPI (`netif_napi_add`, `napi_schedule`,
`napi_complete_done`), `completion`, ACPI `_DSM` (`acpi_evaluate_dsm`, used
for regulatory/SAR settings from BIOS).

### 4.3 Behaviour mac80211 provides implicitly (the hidden dependency)

rtw89 sets `HAS_RATE_CONTROL, AMPDU_AGGREGATION, TX_AMSDU, SUPPORT_FAST_XMIT,
RX_INCLUDES_FCS, MFP_CAPABLE, SUPPORTS_PS, SUPPORTS_DYNAMIC_PS`. It does
**not** set `SUPPORTS_REORDERING_BUFFER` or any RX-offload flag. So mac80211
does all of the following, and on macOS **our shim has to**:

1. **MLME**: auth/assoc/reassoc/deauth/disassoc frame exchange, capability
   negotiation (HT/VHT/HE IEs), beacon tracking, beacon loss, CSA. (Linux:
   `mlme.c`, 12 k lines.)
2. **RX path**: FCS strip, duplicate detection, **A-MPDU reorder buffer**
   per TID (`agg-rx.c`), **A-MSDU de-aggregation**, defragmentation, CCMP/GCMP
   **PN replay check** (hardware decrypts; mac80211 checks PN), 802.11→802.3
   conversion, EAPOL detection. (`rx.c`, 5.8 k lines.)
3. **TX path**: 802.3→802.11 header build, QoS/TID mapping, sequence numbers,
   per-STA TXQs with fq_codel, A-MSDU building (`TX_AMSDU`), ADDBA initiation
   (`agg-tx.c`). (`tx.c`, 6.8 k lines.)
4. **Scan bookkeeping**: building the BSS list from beacons/probe responses
   (rtw89 offloads the channel walk with `hw_scan`).
5. **Power-save decisions**: when to enter LPS (rtw89 implements LPS itself in
   `ps.c` but mac80211 drives `IEEE80211_CONF_PS`).

Firmware does **rate control** (`rtw89_fw_h2c_ra`) and the channel walk for
**hw_scan**, so neither needs a software implementation.

**Supplicant:** on Linux, wpa_supplicant does the 4-way handshake, GTK rekey
and SAE. On macOS, AirportItlwm proved that Apple's **in-kernel RSN
supplicant** can handle WPA2-PSK (`useAppleRSNSupplicant()`,
`APPLE80211_IOC_RSN_IE`, `APPLE80211_IOC_CIPHER_KEY`,
`APPLE80211_M_RSN_HANDSHAKE_DONE`). EAPOL frames go up to the interface and
keys come back down through `CIPHER_KEY` → our `set_key`. **WPA3-SAE is not
solved.** AirportItlwm only maps SAE to WPA2 for mixed-mode networks
(`AirportItlwm.cpp:169-173`). Where SAE commit/confirm runs on macOS
(airportd? IO80211? Broadcom firmware?) is unknown. **[verify]** by
disassembling IO80211Family/airportd on the target macOS. It is listed as
risk R3.

### 4.4 Shim design (`linux_compat/`)

```
linux_compat/
  include/linux/…      header-only: types, bitfield, bitops, list, ewma,
                       unaligned, completion, spinlock→IOSimpleLock,
                       mutex→IOLock, rcu (see below), delay, jiffies
  include/net/mac80211.h, cfg80211.h   clean-room API subset (types + prototypes)
  skb.cpp              sk_buff wrapper around mbuf_t (see §6)
  dma.cpp              dma_map_single / dma_alloc_coherent → IODMACommand /
                       IOBufferMemoryDescriptor
  pci.cpp              pci_* → IOPCIDevice
  firmware.cpp         request_firmware → load from kext bundle Resources
  work.cpp             wiphy_work, delayed_work, workqueue → IOWorkLoop +
                       IOTimerEventSource; wiphy_lock → IOCommandGate
  napi.cpp             NAPI → filter interrupt + poll on the workloop
mac80211_lite/         our own (clean-room) implementations of §4.3 items 1-5:
  mlme.cpp, rx.cpp (reorder, de-amsdu, PN), tx.cpp (txq, fq, amsdu, ba),
  scan.cpp, sta.cpp, key.cpp, frames.cpp (templates)
```

- **RCU:** rtw89 uses RCU for read-mostly STA/vif pointers. First version:
  `rcu_read_lock` = no-op plus a reader counter, and `synchronize_rcu` = wait
  for the counter to drain on the workloop. All writers already run under
  `wiphy_lock` (the gate), so this is safe. Profile it before going further.
- **Concurrency model:** one `IOWorkLoop` per device. The PCIe interrupt uses
  `IOFilterInterruptEventSource`: the filter acks or masks the interrupt, the
  action runs the NAPI-style poll. `wiphy_lock` is the workloop gate, so
  rtw89's locking assumptions hold without code changes.
- **Rebase rule:** files in `rtw89/` are upstream copies plus a short list of
  `#ifdef __APPLE__` hunks, each recorded in `rtw89/PATCHES.md`. Target: under
  ~30 hunks.

---

## 5. IO80211 glue (Apple side)

### 5.1 Two IO80211 generations

| macOS | IO80211 model | Glue class (itlwm precedent) |
|---|---|---|
| ≤ 13 Ventura | `IO80211Controller` + `IO80211Interface` (IONetworking/`outputPacket`) | `AirportItlwm` (V1) |
| 14 Sonoma, 15 Sequoia, 26 Tahoe | IO80211 on **Skywalk** (`IO80211SkywalkInterface`, `IOSkywalkFamily`), Apple80211 requests via `apple80211SkywalkRequest` | `AirportItlwmV2` + `AirportItlwmSkywalkInterface` (231 get/set methods) |

itlwm's Sonoma build links `com.apple.iokit.IO80211Family 1.5.0`,
`IOSkywalkFamily 1.0`, and `com.apple.driver.corecapture`. In practice the
community runs it on Sonoma+ after the **OCLP "Modern Wireless" root patch**,
which reinstalls older IO80211/IOSkywalk binaries. **[verify]** exactly which
binaries OCLP replaces on 14/15/26 and whether a non-patched Skywalk path is
possible at all. itlwm upstream has had no commits since 2024-06, so there is
**no maintained precedent for Sequoia or Tahoe** (risk R1).

Plan: build only the **Skywalk (V2) glue**, because all three targets are
Skywalk-era. Keep the Apple-facing class layer behind a small internal API
(`AppleGlue → Rtw89Core`) so a V1 fallback could be added later. Every
vtable offset or private symbol we depend on goes into one table that is
checked against the running kernel version at `start()`. On mismatch we log
and refuse to attach; we never call through an unverified slot.

### 5.2 Apple80211 requests the Wi-Fi menu and Settings use

itlwm's STA handler covers 59 ioctls. For each one we will log whether
airportd/WiFiAgent calls it on our target versions **[verify on hardware]**.
Required set (number = `APPLE80211_IOC_*` id):

- **Identity/capability:** 12 CARD_CAPABILITIES, 43 DRIVER_VERSION,
  44 HARDWARE_VERSION, 15 OP_MODE, 14 PHY_MODE (11AX = `2<<7`),
  27 SUPPORTED_CHANNELS, 254 HW_SUPPORTED_CHANNELS, 28 LOCALE,
  51 COUNTRY_CODE, 353 NSS, 196 TX_NSS, 345 HE_CAPABILITY, 54 RADIO_INFO
- **Power:** 19 POWER (the Wi-Fi on/off toggle), 5 POWERSAVE
- **Scan:** 10 SCAN_REQ, 86 SCAN_REQ_MULTIPLE, 11 SCAN_RESULT,
  90 SCANCACHE_CLEAR (+ event `M_SCAN_DONE`)
- **Join:** 20 ASSOCIATE, 21 ASSOCIATE_RESULT, 50 ASSOCIATION_STATUS,
  22 DISASSOCIATE, 29 DEAUTH, 2 AUTH_TYPE, 46 RSN_IE, 48 AP_IE_LIST,
  3 CIPHER_KEY, 85 IE (+ events `M_ASSOC_DONE`, `M_LINK_CHANGED`,
  `M_RSN_HANDSHAKE_DONE`, `M_DEAUTH_RECEIVED`, `M_SSID_CHANGED`,
  `M_BSSID_CHANGED`, `M_CIPHER_KEY_CHANGED`)
- **Link info (menu bar, Option-click, Location Services):** 1 SSID,
  9 BSSID, 4 CHANNEL, 13 STATE, 16 RSSI, 17 NOISE, 8 RATE, 7 TXPOWER,
  57 MCS, 66 MCS_INDEX_SET, 181 VHT_MCS_INDEX_SET, 195 MCS_VHT,
  156 LINK_CHANGED_EVENT_DATA, 32 RATE_SET
- **Roaming:** 80 ROAM_THRESH, 107 ROAM, 216 ROAM_PROFILE
- **Coex (Apple side):** 87 BTCOEX_MODE, 221 BTCOEX_PROFILES,
  222 BTCOEX_CONFIG, 235 BTCOEX_OPTIONS (only so Apple's stack is satisfied;
  real coex is in firmware, §8)
- **Sleep:** 69 WOW_PARAMETERS
- **Virtual interfaces (AWDL/P2P):** 94 VIRTUAL_IF_CREATE, 95 DELETE.
  Answer these with a clean "unsupported" until Phase 5.

**6 GHz:** the reverse-engineered Apple80211 headers (itlwm) have no 6 GHz
band or channel flags. How Sonoma+ reports 6 GHz channels is unknown
**[verify]** (risk R4). **HE:** `APPLE80211_MODE_11AX` and `HE_CAPABILITY`
exist, but their struct layouts have to be confirmed.

The Apple80211 headers will be copied from itlwm (GPL-2.0-or-later,
reverse-engineered, not Apple code) with attribution, into
`glue/include/Airport/`.

---

## 6. Data-path design (performance)

| Requirement | Upstream fact | macOS design |
|---|---|---|
| Zero-copy TX | `pci.c` does `dma_map_single(skb->data)` for TX | `sk_buff` wraps the `mbuf_t`; map the mbuf chain with a pre-allocated per-ring `IODMACommand` (64-bit, max segment 4 KiB). The TX WD/BD format takes an address+length per buffer, and chains of more than one segment need WD "addr info" entries (rtw89 already builds those for A-MSDU) **[verify]** |
| Zero-copy RX | RX buffer = `RTW89_PCI_RX_BUF_SIZE` = **11 498 bytes, one DMA segment**, 256 RXBDs max | **Problem:** standard mbuf clusters are 2 KiB/4 KiB. 16 KiB jumbo clusters are not guaranteed physically contiguous **[verify on XNU]**. Options: (a) physically contiguous RX buffers from `IOBufferMemoryDescriptor` and attach them to mbufs as external storage (`mbuf_attachcluster` is not in the public KPI, risk R6); (b) advertise max MPDU 3895 so 4 KiB clusters fit (costs RX A-MSDU size; must measure); (c) copy out of a DMA pool (rejected for the hot path; fallback only). Measure (a) vs (b) in Phase 2 |
| Batched RX / NAPI | `rtw89_pci_napi_poll`, budget `NAPI_POLL_WEIGHT` (64) | Filter interrupt masks the IRQ, poll on the workloop with budget 64, build an mbuf packet chain, one `inputPacket`/Skywalk enqueue per batch, unmask when done |
| Rings | TXBD ≤ 256, TXWD ≤ 512, RXBD ≤ 256 | Start at upstream values, then sweep in Phase 2 (documented in PERF.md) |
| Checksum offload | **rtw89 upstream implements no RX/TX checksum offload** (no `NETIF_F_*CSUM` or csum handling anywhere in the driver) | Advertise none at first. The vendor driver may show whether the RX descriptor carries a checksum-OK bit; that would be a vendor-only quirk **[verify]**. TSO: the hardware has no TSO, so none. This is a gap against the requirement, not a design choice |
| Aggregation | 8852B: `max_tx_agg_num 128`, `max_rx_agg_num 64`, `max_amsdu_limit 5000` | Our `mac80211_lite` TX builds A-MSDU and starts BA. RX reorder runs in our RX path |
| Rate control | firmware RA (`rtw89_fw_h2c_ra`) | Just pass STA capabilities. No software RC |
| PS | `ps.c` LPS; module param `disable_ps_mode` | Boot-arg/IORegistry property `rtw89-ps=off|lps|auto`; default to LPS off while traffic is flowing, matching Linux |
| ASPM | params `disable_clkreq`, `disable_aspm_l1`, `disable_aspm_l1ss` | Expose as properties. Hackintosh firmware often breaks L1SS, so make it easy to turn off |

Logging: `os_log` with a compile-time debug mask. Nothing on the hot path in
RELEASE builds. rtw89's `rtw89_debug()` becomes a macro that is compiled out.

---

## 7. Bluetooth (`RealtekBTFirmware.kext`)

### 7.1 Upstream flow (btrtl.c, verified in source)

1. `btrtl_initialize()`:
   - `0xFC61` vendor read reg16 (`RTL_CHIP_SUBVER`, `RTL_CHIP_REV`) for 8822E
     detection;
   - `HCI_Read_Local_Version` gives `hci_ver, hci_rev, lmp_subver`;
   - `0xFC61` with `{00 94 a0 00 b0}` reads the chip type where
     `rtl_has_chip_type()` applies;
   - `btrtl_match_ic(lmp_subver, hci_rev, hci_ver, bus, chip_type)`. For
     8852B USB: `lmp_subver 0x8852, hci_rev 0xb, hci_ver 0xb`,
     `has_rom_version`, `has_msft_ext`, `config_needed = false`.
2. **Already-patched controller** (warm reboot from Windows/Linux): the
   patched `lmp_subver` no longer matches the ROM table. Upstream then sends
   vendor **`0xFC66` (drop firmware)**, waits 200 ms and retries detection
   once (`drop_fw`). The design follows upstream: always end up running our
   known patch version, rather than skipping. A "skip if already patched"
   mode can be a debug option.
3. `0xFC6D` reads the ROM version, which selects the patch inside the epatch.
4. Parse the firmware: v1 `"Realtech"` or **v2 `"RTBTCore"`** signature
   (8852B family uses v2 subsections, `btrtl_parse_section`).
5. Download with **`0xFC20`** in `RTL_FRAG_LEN` fragments. Each command is
   `index byte + data`. The index counts up, wraps from `0x7f` back to `1`,
   and gets bit `0x80` set on the last fragment.
6. Config blob, if present (`config_needed = false` for 8852B, so optional).
7. Quirks are set and the MSFT extension opcode is registered (Linux-only, not
   needed on macOS).

### 7.2 btusb parts we need

Only the USB transport for the setup phase: find the HCI interface (class
E0/01/01), send HCI commands on the **control endpoint** (bRequest 0, class
request to interface), read events from the **interrupt IN** endpoint. ACL
bulk and SCO isochronous are **not** ours: once the patch is in, Apple's
`IOBluetoothHostControllerUSBTransport` (or its user-space successor on 12+)
takes the device.

### 7.3 macOS integration

- The kext matches the USB **interface/device** with a higher probe score than
  Apple's transport, uploads the patch on its own `IOWorkLoop` (async, with a
  timeout), then **terminates itself / releases the device** so Apple's driver
  matches. This is the IntelBluetoothFirmware model.
- **BlueToolFixup** (Lilu plugin) is required on 12+: it patches `bluetoothd`
  (vendor check against BRCM 0x0A5C, "bad chipset" checks, NVRAM internal
  controller check). These are **byte patches tied to specific macOS
  versions**, maintained by acidanthera. We depend on BlueToolFixup and do not
  fork it. Its README also requires NVRAM
  `bluetoothExternalDongleFailed=00` and `bluetoothInternalControllerInfo=0…`.
- Re-upload triggers: USB re-enumeration (the kext matches again), system wake
  (`IOPMPowerState` change plus a check of `lmp_subver` to see if the patch
  was lost), and a USB port reset.
- **No public evidence exists of Realtek BT working with the macOS 12+ stack.**
  Intel works (partially) with BlueToolFixup. Realtek's HCI behaviour under
  Apple's bluetoothd (LE feature set, vendor commands Apple sends to what it
  thinks is a Broadcom controller, SCO/eSCO over isochronous USB) is untested
  (risk R7).

---

## 8. Coexistence (BTC)

Verified in `coex.c`: BT state reaches the Wi-Fi driver **through Wi-Fi
firmware C2H events** (`BTF_EVNT_BT_INFO` → `_update_bt_info()`), and the
Wi-Fi/BT **scoreboard** is written over H2C (`_write_scbd`). The combo die's
two firmwares exchange state internally. So:

- **No IPC between the two kexts is needed for coex in principle.** Port
  `coex.c` (14.5 k lines) unchanged. It only needs fw/H2C/C2H, timers
  (`wiphy_delayed_work`) and the `rtw89_btc_ntfy_*` hooks.
- Our glue must call the Wi-Fi-side notification hooks at the right moments:
  `rtw89_btc_ntfy_poweron/off`, `_init`, `_scan_start/_finish`,
  `_switch_band`, `_role_info` (connect/disconnect), `_radio_state`,
  `_wl_sta`, `_specific_packet` (EAPOL/DHCP/ARP/ICMP, so coex protects them),
  `_wl_rfk`. These already sit in rtw89 core paths that we keep.
- A small shared IOService (`Rtw89CoexBridge`) is only a fallback, if firmware
  doesn't report profiles (A2DP/HID) well enough, which shows up as
  `cx->bt*` profile bits staying empty in the `Rtw89Tool` dump **[verify]**.
- Antenna/BT RF config comes from efuse and chip tables. Nothing
  macOS-specific there.

---

## 9. Power management

- Wi-Fi: `IOPMPowerState` with two states. Sleep → `rtw89_ops_suspend` (WoWLAN
  disabled at first, so we run full `stop`). Wake → `start` + reconnect via
  Apple's auto-join. GTK rekey offload (`set_rekey_data`) only matters once
  WoWLAN is enabled.
- BT: on wake, check whether the patch survived; re-upload if not.
- Runtime PS: LPS through rtw89 `ps.c`, controlled by the IO80211 POWERSAVE
  ioctl and the property in §6.

---

## 10. Blockers per macOS version (current knowledge)

| | 14 Sonoma | 15 Sequoia | 26 Tahoe |
|---|---|---|---|
| IO80211 attach | Skywalk V2 glue; itlwm precedent with OCLP root patch | No upstream itlwm precedent | No precedent at all; last Intel release, IO80211 changes unknown |
| Root patching | OCLP Modern Wireless **[verify]** | OCLP **[verify]** | OCLP support for Tahoe **[verify]** |
| SIP/AMFI | Root patch requires reduced SIP (and AMFI changes for some patches) **[verify]** | same | same |
| WPA3-SAE | unsolved (R3) | unsolved | unsolved |
| 6 GHz | ioctl layout unknown (R4) | same | same |
| BT stack | BlueToolFixup supports **[verify version]** | **[verify]** | **[verify]** |

Each **[verify]** cell gets resolved on real hardware in Phase 1/2 and
recorded in `STATUS.md`.

---

## 11. Repository layout (planned)

```
AirportRtw89/          Wi-Fi kext
  glue/                IO80211 Skywalk controller + interface, Apple80211 ioctls
  mac80211_lite/       clean-room 802.11 STA logic (§4.3)
  linux_compat/        thin kernel/mac80211 API shim (§4.4)
  rtw89/               upstream driver files (8852B family first) + PATCHES.md
  Resources/           firmware + firmware licence
RealtekBTFirmware/     BT kext: btrtl port + minimal USB HCI transport
Rtw89Tool/             userspace CLI (IOUserClient): fw ver, BTC dump, ring stats
docs/                  ARCHITECTURE, RISKS, STATUS, PERF, AWDL_FEASIBILITY, CONTINUITY
.github/workflows/     Xcode build: DEBUG and RELEASE
```

Build: Xcode + acidanthera **MacKernelSDK** (as itlwm does) on a GitHub
`macos` runner. Lilu is only needed if we add our own patches; BlueToolFixup
is a separate kext.

---

## 12. Hardware information needed before Phase 1

1. Exact Wi-Fi chip and PCI ID (`lspci -nn` or IORegistry `device-id`,
   `subsystem-id`, `revision-id`).
2. BT USB VID:PID (`system_profiler SPUSBDataType` or `lsusb`).
3. macOS version(s) and build numbers you will test on.
4. SMBIOS model; OpenCore version; SIP / `csr-active-config`; whether OCLP root
   patches are installed today.
5. Motherboard/laptop model (ACPI `_DSM` and ASPM behaviour depend on it).
6. Whether Linux can be booted on the same machine for reference numbers, and
   which AP (model, bands, 160 MHz/6 GHz support).
