/* SPDX-License-Identifier: GPL-3.0-only */
#include "Rtw89PCIProbe.hpp"

#include <IOKit/IOLib.h>
#include <libkern/OSByteOrder.h>

#define super IOService
OSDefineMetaClassAndStructors(Rtw89PCIProbe, IOService)

#define RLOG(fmt, ...) IOLog("rtw89: " fmt "\n", ##__VA_ARGS__)

/*
 * Register facts, all from upstream rtw89 (torvalds/linux f66469d9905b):
 *   reg.h:  R_AX_SYS_CFG1 0x00F0, B_AX_CHIP_VER_MASK GENMASK(15, 12)
 *   core.h: CHIP_CAV = 0, CHIP_CBV, CHIP_CCV, ... ("A-cut", "B-cut", ...)
 *   core.h: RTW89_R32_EA 0xEAEAEAEA, RTW89_R32_DEAD 0xDEADBEEF
 *   pci.c:  MMIO is BAR index 2
 */
static constexpr UInt32 kRegSysCfg1    = 0x00F0;
static constexpr UInt32 kChipVerShift  = 12;
static constexpr UInt32 kChipVerMask   = 0xFu << kChipVerShift;
static constexpr UInt32 kR32Ea         = 0xEAEAEAEA;
static constexpr UInt32 kR32Dead       = 0xDEADBEEF;
static constexpr UInt16 kVendorRealtek = 0x10EC;

struct ChipName {
    UInt16 device;
    const char *name;
};

/* PCI device IDs from the upstream rtw89/rtw8*e.c ID tables. */
static const ChipName kChips[] = {
    { 0x8852, "RTL8852AE" }, { 0xA85A, "RTL8852AE" },
    { 0xB852, "RTL8852BE" }, { 0xB85B, "RTL8852BE" },
    { 0xB520, "RTL8852BE-VT (8852BT)" },
    { 0xC852, "RTL8852CE" },
    { 0xB851, "RTL8851BE" },
    { 0x8922, "RTL8922AE" }, { 0x892B, "RTL8922AE" },
};

static const char *chipName(UInt16 device)
{
    for (const ChipName &c : kChips)
        if (c.device == device)
            return c.name;
    return "unknown";
}

bool Rtw89PCIProbe::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    fPci = OSDynamicCast(IOPCIDevice, provider);
    if (!fPci) {
        RLOG("provider is not an IOPCIDevice");
        super::stop(provider);
        return false;
    }
    fPci->retain();

    if (!fPci->open(this)) {
        RLOG("could not open PCI provider (already in use?)");
        releaseResources();
        super::stop(provider);
        return false;
    }
    fOpened = true;

    const UInt16 vendor = fPci->configRead16(kIOPCIConfigVendorID);
    const UInt16 device = fPci->configRead16(kIOPCIConfigDeviceID);
    const UInt16 subVendor = fPci->configRead16(kIOPCIConfigSubSystemVendorID);
    const UInt16 subDevice = fPci->configRead16(kIOPCIConfigSubSystemID);
    const UInt8 revision = fPci->configRead8(kIOPCIConfigRevisionID);

    RLOG("PCI %04x:%04x subsys %04x:%04x rev %02x (%s) at %u:%u.%u",
         vendor, device, subVendor, subDevice, revision, chipName(device),
         fPci->getBusNumber(), fPci->getDeviceNumber(),
         fPci->getFunctionNumber());

    if (vendor != kVendorRealtek) {
        RLOG("not a Realtek device, detaching");
        releaseResources();
        super::stop(provider);
        return false;
    }

    /* Remember the command register so stop() leaves it as we found it. */
    fSavedCommand = fPci->configRead16(kIOPCIConfigCommand);
    fCommandSaved = true;
    fPci->setMemoryEnable(true);

    fBar = fPci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress2);
    if (!fBar) {
        RLOG("BAR2 is not a mappable memory BAR, detaching");
        releaseResources();
        super::stop(provider);
        return false;
    }

    const volatile UInt8 *mmio =
        reinterpret_cast<const volatile UInt8 *>(fBar->getVirtualAddress());
    const UInt32 sysCfg1 =
        OSReadLittleInt32(const_cast<const UInt8 *>(mmio), kRegSysCfg1);

    RLOG("BAR2 phys 0x%llx len 0x%llx, SYS_CFG1 = 0x%08x",
         (unsigned long long)fBar->getPhysicalAddress(),
         (unsigned long long)fBar->getLength(), sysCfg1);

    if (sysCfg1 == 0xFFFFFFFF || sysCfg1 == kR32Ea || sysCfg1 == kR32Dead) {
        /* All-ones means the read did not reach the device (link down,
         * device in D3cold, wrong BAR). Report it; don't guess. */
        RLOG("SYS_CFG1 read returned an invalid pattern; chip not reachable");
        setProperty("rtw89-probe-status", "mmio-unreachable");
    } else {
        const UInt32 cv = (sysCfg1 & kChipVerMask) >> kChipVerShift;
        char cut[2] = { static_cast<char>('A' + cv), 0 };
        RLOG("%s, chip cut %s (cv=%u)", chipName(device), cut, cv);
        setProperty("rtw89-chip-cut", cut);
        setProperty("rtw89-chip-cv", cv, 32);
        setProperty("rtw89-probe-status", "ok");
    }

    setProperty("rtw89-chip", chipName(device));
    setProperty("rtw89-sys-cfg1", sysCfg1, 32);
    setProperty("rtw89-subsystem", (UInt32(subDevice) << 16) | subVendor, 32);
    setProperty("rtw89-revision", revision, 8);

    registerService();
    return true;
}

void Rtw89PCIProbe::stop(IOService *provider)
{
    RLOG("stopping");
    releaseResources();
    super::stop(provider);
}

void Rtw89PCIProbe::releaseResources()
{
    OSSafeReleaseNULL(fBar);
    if (fPci) {
        if (fOpened) {
            /* Restore only the memory-space bit we may have changed. */
            if (fCommandSaved && !(fSavedCommand & kIOPCICommandMemorySpace))
                fPci->setMemoryEnable(false);
            fCommandSaved = false;
            fPci->close(this);
            fOpened = false;
        }
        OSSafeReleaseNULL(fPci);
    }
}
