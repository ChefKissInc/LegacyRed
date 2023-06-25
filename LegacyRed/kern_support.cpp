//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_support.hpp"
#include "kern_lred.hpp"
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
        auto highsierra = getKernelVersion() == KernelVersion::HighSierra;

        RouteRequestPlus requests[] = {
            {"__ZN13ATIController20populateDeviceMemoryE13PCI_REG_INDEX", wrapPopulateDeviceMemory,
                orgPopulateDeviceMemory},
            {"__ZN16AtiDeviceControl16notifyLinkChangeE31kAGDCRegisterLinkControlEvent_tmj", wrapNotifyLinkChange,
                orgNotifyLinkChange},
            {"__ZN13ATIController8TestVRAME13PCI_REG_INDEXb", doNotTestVram},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "support",
            "Failed to route symbols");

        LookupPatchPlus const patches[] = {
            {&kextRadeonSupport, kVRAMInfoNullCheckOriginal, kVRAMInfoNullCheckPatched,
                arrsize(kVRAMInfoNullCheckOriginal), 1, !highsierra},
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
    return true;
}
