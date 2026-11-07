/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Phase 1a: read-only PCIe probe.
 *
 * Attaches to a Realtek rtw89 PCIe function, maps BAR2 (the MMIO BAR upstream
 * rtw89 uses, pci.c: bar_id = 2), reads identification registers that are
 * readable without powering on the MAC, publishes the result in the IORegistry
 * and logs it. It never writes a device register, never enables bus
 * mastering, never allocates DMA memory and never registers an interrupt, so
 * it can't corrupt memory or wedge the bus.
 *
 * This class is replaced by the real controller in Phase 1b, and the
 * register-reading parts move into the linux_compat/rtw89 path.
 */
#ifndef RTW89_PCI_PROBE_HPP
#define RTW89_PCI_PROBE_HPP

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOMemoryDescriptor.h>

class Rtw89PCIProbe : public IOService {
    OSDeclareDefaultStructors(Rtw89PCIProbe)

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;

private:
    IOPCIDevice *fPci = nullptr;
    IOMemoryMap *fBar = nullptr;
    UInt16 fSavedCommand = 0;
    bool fCommandSaved = false;
    bool fOpened = false;

    void releaseResources();
    void logProviderClients();
};

#endif
