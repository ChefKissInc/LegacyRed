//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_gfxcon.hpp"
#include "kern_lred.hpp"
#include "kern_patches.hpp"
#include <Headers/kern_api.hpp>

static const char *pathRadeonGFX7Con =
    "/System/Library/Extensions/AMD8000Controller.kext/Contents/MacOS/AMD8000Controller";

static const char *pathRadeonGFX8Con =
    "/System/Library/Extensions/AMD9000Controller.kext/Contents/MacOS/AMD9000Controller";

static KernelPatcher::KextInfo kextRadeonGFX7Con = {"com.apple.kext.AMD8000Controller", &pathRadeonGFX7Con, 1, {}, {},
    KernelPatcher::KextInfo::Unloaded};

static KernelPatcher::KextInfo kextRadeonGFX8Con = {"com.apple.kext.AMD9000Controller", &pathRadeonGFX8Con, 1, {}, {},
    KernelPatcher::KextInfo::Unloaded};

GFXCon *GFXCon::callback = nullptr;

void GFXCon::init() {
    callback = this;
    lilu.onKextLoadForce(&kextRadeonGFX7Con);
}

bool GFXCon::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextRadeonGFX7Con.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();

        RouteRequestPlus requests[] = {
            {"__ZNK18CISharedController11getFamilyIdEv", wrapGetFamilyId, orgGetFamilyId},
            {"__ZN17CIRegisterService11hwReadReg32Ej", wrapHwReadReg32, this->orgHwReadReg32},
            {"__ZN13ASIC_INFO__CI18populateDeviceInfoEv", wrapPopulateDeviceInfo, this->orgPopulateDeviceInfo},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "gfxcon",
            "Failed to route symbols");

        return true;
    } else if (kextRadeonGFX8Con.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();

        RouteRequestPlus requests[] = {
            {"__ZNK18VISharedController11getFamilyIdEv", wrapGetFamilyId, orgGetFamilyId},
            {"__ZN17VIRegisterService11hwReadReg32Ej", wrapHwReadReg32, this->orgHwReadReg32},
            {"__ZN13ASIC_INFO__VI18populateDeviceInfoEv", wrapPopulateDeviceInfo, this->orgPopulateDeviceInfo},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "gfxcon",
            "Failed to route symbols");

        return true;
    }

    return false;
}

uint32_t GFXCon::wrapHwReadReg32(void *that, uint32_t reg) {
    return FunctionCast(wrapHwReadReg32, callback->orgHwReadReg32)(that, reg == 0xD31 ? 0xD2F : reg);
}

uint16_t GFXCon::wrapGetFamilyId(void) {
    FunctionCast(wrapGetFamilyId, callback->orgGetFamilyId)();
    DBGLOG("gfxcon", "getFamilyId >> %x", LRed::callback->isGcn3Derivative ? AMDGPU_FAMILY_CZ : AMDGPU_FAMILY_KV);
    return LRed::callback->isGcn3Derivative ? AMDGPU_FAMILY_CZ : AMDGPU_FAMILY_KV;
}

IOReturn GFXCon::wrapPopulateDeviceInfo(void *that) {
    auto ret = FunctionCast(wrapPopulateDeviceInfo, callback->orgPopulateDeviceInfo)(that);
    getMember<uint32_t>(that, 0x60) = LRed::callback->isGcn3Derivative ? AMDGPU_FAMILY_CZ : AMDGPU_FAMILY_KV;
    return ret;
}
