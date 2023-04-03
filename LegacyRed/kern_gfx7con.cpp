//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

//  Copyright © 2023 Zormeister. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_gfx7con.hpp"
#include "kern_lred.hpp"
#include "kern_patches.hpp"
#include <Headers/kern_api.hpp>

static const char *pathRadeonGFX7Con =
    "/System/Library/Extensions/AMD8000Controller.kext/Contents/MacOS/AMD8000Controller";

static KernelPatcher::KextInfo kextRadeonGFX7Con = {"com.apple.kext.AMD8000Controller", &pathRadeonGFX7Con, 1, {}, {},
    KernelPatcher::KextInfo::Unloaded};

GFX7Con *GFX7Con::callback = nullptr;

void GFX7Con::init() {
    callback = this;
    lilu.onKextLoadForce(&kextRadeonGFX7Con);
}

bool GFX7Con::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextRadeonGFX7Con.loadIndex == index) {
        KernelPatcher::RouteRequest requests[] = {
            {"__ZNK18CISharedController11getFamilyIdEv", wrapGetFamilyId, orgGetFamilyId},
            {"__ZN17CIRegisterService11hwReadReg32Ej", wrapHwReadReg32, this->orgHwReadReg32},
			{"__ZN13ASIC_INFO__CI18populateDeviceInfoEv", wrapPopulateDeviceInfo, this->orgPopulateDeviceInfo},
        };
        PANIC_COND(!patcher.routeMultiple(index, requests, address, size), "gfx7con", "Failed to route symbols");

        return true;
    }

    return false;
}

uint32_t GFX7Con::wrapHwReadReg32(void *that, uint32_t reg) {
    return FunctionCast(wrapHwReadReg32, callback->orgHwReadReg32)(that, reg == 0xD31 ? 0xD2F : reg);
}

uint16_t GFX7Con::wrapGetFamilyId(void) {
    FunctionCast(wrapGetFamilyId, callback->orgGetFamilyId)();
    DBGLOG("gfx7con", "getFamilyId >> 0x7D");
    return 0x7D;
}

IOReturn GFX7Con::wrapPopulateDeviceInfo(void *that) {
	auto ret = FunctionCast(wrapPopulateDeviceInfo, callback->orgPopulateDeviceInfo)(that);
	getMember<uint32_t>(that, 0x60) = AMDGPU_FAMILY_KV;
	return ret;
}
