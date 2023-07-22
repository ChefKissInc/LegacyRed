//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_support.hpp"
#include "kern_lred.hpp"
#include "kern_model.hpp"
#include "kern_patches.hpp"
#include <Headers/kern_api.hpp>

static const char *pathRadeonSupport = "/System/Library/Extensions/AMDSupport.kext/Contents/MacOS/AMDSupport";

static KernelPatcher::KextInfo kextRadeonSupport = {"com.apple.kext.AMDSupport", &pathRadeonSupport, 1, {}, {},
    KernelPatcher::KextInfo::Unloaded};

Support *Support::callback = nullptr;

void Support::init() {
    callback = this;
    lilu.onKextLoadForce(&kextRadeonSupport);
}

bool Support::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextRadeonSupport.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();

        RouteRequestPlus requests[] = {
            {"__ZN13ATIController20populateDeviceMemoryE13PCI_REG_INDEX", wrapPopulateDeviceMemory,
                orgPopulateDeviceMemory},
            {"__ZN16AtiDeviceControl16notifyLinkChangeE31kAGDCRegisterLinkControlEvent_tmj", wrapNotifyLinkChange,
                orgNotifyLinkChange},
            {"__ZN13ATIController8TestVRAME13PCI_REG_INDEXb", doNotTestVram},
            {"__ZN30AtiObjectInfoTableInterface_V120getAtomConnectorInfoEjRNS_17AtomConnectorInfoE",
                wrapGetAtomConnectorInfo, orgGetAtomConnectorInfo},
            {"__ZN30AtiObjectInfoTableInterface_V121getNumberOfConnectorsEv", wrapGetNumberOfConnectors,
                orgGetNumberOfConnectors},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "support",
            "Failed to route symbols");

        LookupPatchPlus const patches[] = {
            {&kextRadeonSupport, kAtiDeviceControlGetVendorInfoOriginal, kAtiDeviceControlGetVendorInfoMask,
                kAtiDeviceControlGetVendorInfoPatched, kAtiDeviceControlGetVendorInfoMask,
                arrsize(kAtiDeviceControlGetVendorInfoOriginal), 1},
        };
        PANIC_COND(!LookupPatchPlus::applyAll(&patcher, patches, address, size), "support",
            "Failed to apply patches: %d", patcher.getError());

        return true;
    }

    return false;
}

IOReturn Support::wrapPopulateDeviceMemory(void *that, uint32_t reg) {
    DBGLOG("support", "populateDeviceMemory: this = %p reg = 0x%X", that, reg);
    auto ret = FunctionCast(wrapPopulateDeviceMemory, callback->orgPopulateDeviceMemory)(that, reg);
    DBGLOG("support", "populateDeviceMemory returned 0x%X", ret);
    (void)ret;
    return kIOReturnSuccess;
}

bool Support::wrapNotifyLinkChange(void *atiDeviceControl, kAGDCRegisterLinkControlEvent_t event, void *eventData,
    uint32_t eventFlags) {
    auto ret = FunctionCast(wrapNotifyLinkChange, callback->orgNotifyLinkChange)(atiDeviceControl, event, eventData,
        eventFlags);

    if (event == kAGDCValidateDetailedTiming) {
        auto cmd = static_cast<AGDCValidateDetailedTiming_t *>(eventData);
        DBGLOG("support", "AGDCValidateDetailedTiming %u -> %d (%u)", cmd->framebufferIndex, ret, cmd->modeStatus);
        if (ret == false || cmd->modeStatus < 1 || cmd->modeStatus > 3) {
            cmd->modeStatus = 2;
            ret = true;
        }
    }

    return ret;
}

bool Support::doNotTestVram([[maybe_unused]] IOService *ctrl, [[maybe_unused]] uint32_t reg,
    [[maybe_unused]] bool retryOnFail) {
    DBGLOG("support", "TestVRAM called! Returning true");
    auto *model = getBranding(LRed::callback->deviceId, LRed::callback->pciRevision);
    // Why do we set it here?
    // Our controller kexts override it, and since TestVRAM is later on after the controllers have started up, it's
    // desirable to do it here rather than later
    if (model) {
        auto len = static_cast<uint32_t>(strlen(model) + 1);
        LRed::callback->iGPU->setProperty("model", const_cast<char *>(model), len);
        if (LRed::callback->chipType >= ChipType::Carrizo) {
            // ATY,FamilyName and ATY,DeviceName appears to only be set on Polaris+? confirmation needed
            LRed::callback->iGPU->setProperty("ATY,FamilyName", const_cast<char *>("Radeon"), 7);
            LRed::callback->iGPU->setProperty("ATY,DeviceName", const_cast<char *>(model) + 11,
                len - 11); /** TODO: Figure out if this works on LRed or not */
        }
    }
    return true;
}

IOReturn Support::wrapGetAtomConnectorInfo(void *that, uint32_t connector, AtomConnectorInfo *coninfo) {
    DBGLOG("support", "getAtomConnectorInfo: connector %x", connector);
    auto ret = FunctionCast(wrapGetAtomConnectorInfo, callback->orgGetAtomConnectorInfo)(that, connector, coninfo);
    DBGLOG("support", "getAtomConnectorInfo: returned %x", ret);
    return ret;
}

uint32_t Support::wrapGetNumberOfConnectors(void *that) {
    auto ret = FunctionCast(wrapGetNumberOfConnectors, callback->orgGetNumberOfConnectors)(that);
    DBGLOG("support", "getNumberOfConnectors returned: %x", ret);
    unsigned int conCountOverride;
    if (PE_parse_boot_argn("lredconoverride", &conCountOverride, sizeof(conCountOverride))) {
        // temporary fix for my main machine, still needs to be fixed, but, here for now.
        // AtomConnectorInfo should have all we need in the future.
        DBGLOG("support", "Connector Count override selected, using a count of %x", conCountOverride);
        getMember<unsigned int>(that, 0x10) = conCountOverride;
        return conCountOverride;
    }
    return ret;
}
