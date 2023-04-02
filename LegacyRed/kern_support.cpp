//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

//  Copyright © 2023 Zormeister. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
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
        KernelPatcher::RouteRequest requests[] = {
            {"__ZN13ATIController20populateDeviceMemoryE13PCI_REG_INDEX", wrapPopulateDeviceMemory, orgPopulateDeviceMemory},
        };
        PANIC_COND(!patcher.routeMultiple(index, requests, address, size), "support", "Failed to route symbols");

        KernelPatcher::LookupPatch patches[] = {
            {&kextRadeonSupport, kVRAMInfoNullCheckOriginal, kVRAMInfoNullCheckPatched,
                arrsize(kVRAMInfoNullCheckOriginal), 1},
        };
        for (auto &patch : patches) {
            patcher.applyLookupPatch(&patch);
            patcher.clearError();
        }

        return true;
    }

    return false;
}
/** Wrap is likely broken, don't be surprised if it is */
IOReturn Support::wrapPopulateDeviceMemory(void *that, uint32_t reg) {
	DBGLOG("support", "populateDeviceMemory: this = %p reg = 0x%X", that, reg);
	auto ret = FunctionCast(wrapPopulateDeviceMemory, callback->orgPopulateDeviceMemory)(that, reg);
	DBGLOG("support", "populateDeviceMemory returned 0x%X", ret);
	return kIOReturnSuccess;
}
