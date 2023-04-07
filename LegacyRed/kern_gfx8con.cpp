//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

//  Copyright © 2023 Zormeister. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_gfx8con.hpp"
#include "kern_lred.hpp"
#include "kern_patches.hpp"
#include <Headers/kern_api.hpp>

static const char *pathRadeonGFX8Con =
    "/System/Library/Extensions/AMD9000Controller.kext/Contents/MacOS/AMD9000Controller";

static KernelPatcher::KextInfo kextRadeonGFX8Con = {"com.apple.kext.AMD9000Controller", &pathRadeonGFX8Con, 1, {}, {},
    KernelPatcher::KextInfo::Unloaded};

GFX8Con *GFX8Con::callback = nullptr;

void GFX8Con::init() {
    callback = this;
    lilu.onKextLoadForce(&kextRadeonGFX8Con);
}

bool GFX8Con::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextRadeonGFX8Con.loadIndex == index) {
        KernelPatcher::RouteRequest requests[] = {
            {"__ZNK18VISharedController11getFamilyIdEv", wrapGetFamilyId, orgGetFamilyId},
            {"__ZN17VIRegisterService11hwReadReg32Ej", wrapHwReadReg32, this->orgHwReadReg32},
            {"__ZN13ASIC_INFO__VI18populateDeviceInfoEv", wrapPopulateDeviceInfo, this->orgPopulateDeviceInfo},
        };
        PANIC_COND(!patcher.routeMultiple(index, requests, address, size), "gfx8con", "Failed to route symbols");

        return true;
    }

    return false;
}

uint32_t GFX8Con::wrapHwReadReg32(void *that, uint32_t reg) {
    return FunctionCast(wrapHwReadReg32, callback->orgHwReadReg32)(that, reg == 0xD31 ? 0xD2F : reg);
}

uint16_t GFX8Con::wrapGetFamilyId(void) {
    FunctionCast(wrapGetFamilyId, callback->orgGetFamilyId)();
    DBGLOG("gfx8con", "getFamilyId >> 0x87");
    return 0x87;
}

IOReturn GFX8Con::wrapPopulateDeviceInfo(void *that) {
    auto ret = FunctionCast(wrapPopulateDeviceInfo, callback->orgPopulateDeviceInfo)(that);
    getMember<uint32_t>(that, 0x60) = AMDGPU_FAMILY_CZ;
    return ret;
}
