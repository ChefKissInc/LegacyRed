//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.5. See LICENSE for
//  details.

#include "kern_gfxcon.hpp"
#include "kern_lred.hpp"
#include "kern_patcherplus.hpp"
#include "kern_patches.hpp"
#include <Headers/kern_api.hpp>

static const char *pathRadeonGFX7Con =
    "/System/Library/Extensions/AMD8000Controller.kext/Contents/MacOS/AMD8000Controller";

static const char *pathRadeonGFX8Con =
    "/System/Library/Extensions/AMD9000Controller.kext/Contents/MacOS/AMD9000Controller";

static const char *pathRadeonPolarisCon =
    "/System/Library/Extensions/AMD9500Controller.kext/Contents/MacOS/AMD9500Controller";

static KernelPatcher::KextInfo kextRadeonGFX7Con = {"com.apple.kext.AMD8000Controller", &pathRadeonGFX7Con, 1, {}, {},
    KernelPatcher::KextInfo::Unloaded};

static KernelPatcher::KextInfo kextRadeonGFX8Con = {"com.apple.kext.AMD9000Controller", &pathRadeonGFX8Con, 1, {}, {},
    KernelPatcher::KextInfo::Unloaded};

static KernelPatcher::KextInfo kextRadeonPolarisCon = {"com.apple.kext.AMD9500Controller", &pathRadeonPolarisCon, 1, {},
    {}, KernelPatcher::KextInfo::Unloaded};

GFXCon *GFXCon::callback = nullptr;

void GFXCon::init() {
    callback = this;
    lilu.onKextLoadForce(&kextRadeonGFX7Con);
    lilu.onKextLoadForce(&kextRadeonGFX8Con);
    lilu.onKextLoadForce(&kextRadeonPolarisCon);
}

bool GFXCon::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextRadeonGFX7Con.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();
        auto highsierra = getKernelVersion() == KernelVersion::HighSierra;
        auto regDbg = checkKernelArgument("-lredregdbg");

        RouteRequestPlus requests[] = {
            {"__ZN18CISharedController11getFamilyIdEv", wrapGetFamilyId, orgGetFamilyId, highsierra},
            {"__ZN17CIRegisterService10hwReadReg8Ej", wrapHwReadReg8, this->orgHwReadReg8, regDbg},
            {"__ZN17CIRegisterService11hwReadReg16Ej", wrapHwReadReg16, this->orgHwReadReg16, regDbg},
            {"__ZN17CIRegisterService11hwReadReg32Ej", wrapHwReadReg32, this->orgHwReadReg32, regDbg},
            {"__ZN13ASIC_INFO__CI18populateDeviceInfoEv", wrapPopulateDeviceInfo, this->orgPopulateDeviceInfo,
                !highsierra},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "gfxcon",
            "Failed to route symbols");
/*
        LookupPatchPlus const patches[] = {
            //{&kextRadeonGFX7Con, kAsicInfoCIPopulateDeviceInfoOriginal, kAsicInfoCIPopulateDeviceInfoPatched,
                //arrsize(kAsicInfoCIPopulateDeviceInfoOriginal), 1},
            {&kextRadeonGFX7Con, kCISharedControllerGetFamilyIdOriginal, kCISharedControllerGetFamilyIdPatched,
                arrsize(kCISharedControllerGetFamilyIdOriginal), 1},
        };
        PANIC_COND(!LookupPatchPlus::applyAll(&patcher, patches, address, size), "gfxcon",
            "Failed to apply patches: %d", patcher.getError());
        DBGLOG("gfxcon", "Applied patches.");
*/
        return true;
    } else if (kextRadeonGFX8Con.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();
        auto highsierra = getKernelVersion() == KernelVersion::HighSierra;
        auto regDbg = checkKernelArgument("-lredregdbg");

        RouteRequestPlus requests[] = {
            {"__ZN18VISharedController11getFamilyIdEv", wrapGetFamilyId, orgGetFamilyId, highsierra},
            {"__ZN17VIRegisterService10hwReadReg8Ej", wrapHwReadReg8, this->orgHwReadReg8, regDbg},
            {"__ZN17VIRegisterService11hwReadReg16Ej", wrapHwReadReg16, this->orgHwReadReg16, regDbg},
            {"__ZN17VIRegisterService11hwReadReg32Ej", wrapHwReadReg32, this->orgHwReadReg32, regDbg},
            {"__ZN13ASIC_INFO__VI18populateDeviceInfoEv", wrapPopulateDeviceInfo, this->orgPopulateDeviceInfo,
                !highsierra},
            {"__ZN17AMD9000Controller11getPllClockEhP11ClockParams", wrapGetPllClock, orgGetPllClock},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "gfxcon",
            "Failed to route symbols");
/*
        LookupPatchPlus const patches[] = {
            //{&kextRadeonGFX8Con, kAsicInfoVIPopulateDeviceInfoOriginal, kAsicInfoVIPopulateDeviceInfoPatched,
                //arrsize(kAsicInfoVIPopulateDeviceInfoOriginal), 1},
            {&kextRadeonGFX8Con, kVIBaffinSharedControllerGetFamilyIdOriginal,
                kVIBaffinSharedControllerGetFamilyIdPatched, arrsize(kVIBaffinSharedControllerGetFamilyIdOriginal), 1},
        };
        PANIC_COND(!LookupPatchPlus::applyAll(&patcher, patches, address, size), "gfxcon",
            "Failed to apply patches: %d", patcher.getError());
        DBGLOG("gfxcon", "Applied patches.");
*/
        return true;
    } else if (kextRadeonPolarisCon.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();
        auto highsierra = getKernelVersion() == KernelVersion::HighSierra;
        auto regDbg = checkKernelArgument("-lredregdbg");

        RouteRequestPlus requests[] = {
            {"__ZNK22BaffinSharedController11getFamilyIdEv", wrapGetFamilyId, orgGetFamilyId, !highsierra},
            {"__ZN21BaffinRegisterService10hwReadReg8Ej", wrapHwReadReg8, this->orgHwReadReg8, regDbg},
            {"__ZN21BaffinRegisterService11hwReadReg16Ej", wrapHwReadReg16, this->orgHwReadReg16, regDbg},
            {"__ZN21BaffinRegisterService11hwReadReg32Ej", wrapHwReadReg32, this->orgHwReadReg32, regDbg},
            {"__ZN17ASIC_INFO__BAFFIN18populateDeviceInfoEv", wrapPopulateDeviceInfo, this->orgPopulateDeviceInfo,
                !highsierra},
            {"__ZN22BaffinSharedController6regr32Ej", wrapRegr32, orgRegr32, regDbg},
            {"__ZN22BaffinSharedController6regr16Ej", wrapRegr16, orgRegr16, regDbg},
            {"__ZN22BaffinSharedController5regr8Ej", wrapRegr8, orgRegr8, regDbg},
            {"__ZN17AMD9500Controller11getPllClockEhP11ClockParams", wrapGetPllClock, orgGetPllClock},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "gfxcon",
            "Failed to route symbols");
/*
        LookupPatchPlus const patches[] = {
            //{&kextRadeonPolarisCon, kAsicInfoVIPopulateDeviceInfoOriginal, kAsicInfoVIPopulateDeviceInfoPatched,
                //arrsize(kAsicInfoVIPopulateDeviceInfoOriginal), 1},
            {&kextRadeonPolarisCon, kVIBaffinSharedControllerGetFamilyIdOriginal,
                kVIBaffinSharedControllerGetFamilyIdPatched, arrsize(kVIBaffinSharedControllerGetFamilyIdOriginal), 1},
        }; 
        PANIC_COND(!LookupPatchPlus::applyAll(&patcher, patches, address, size), "gfxcon",
            "Failed to apply patches: %d", patcher.getError());
        DBGLOG("gfxcon", "Applied patches.");
*/
        return true;
    }

    return false;
}

uint8_t GFXCon::wrapHwReadReg8(void *that, uint8_t reg) {
    DBGLOG("gfxcon", "readReg8: reg: %x", reg);
    // turned on by using -lredregdbg
    return FunctionCast(wrapHwReadReg8, callback->orgHwReadReg8)(that, reg);
}

uint16_t GFXCon::wrapHwReadReg16(void *that, uint16_t reg) {
    DBGLOG("gfxcon", "readReg16: reg: %x", reg);
    // turned on by using -lredregdbg
    return FunctionCast(wrapHwReadReg16, callback->orgHwReadReg16)(that, reg);
}

uint32_t GFXCon::wrapHwReadReg32(void *that, uint32_t reg) {
    DBGLOG("gfxcon", "readReg32: reg: %x", reg);
    // turned on by using -lredregdbg
    return FunctionCast(wrapHwReadReg32, callback->orgHwReadReg32)(that, reg);
}

uint8_t GFXCon::wrapRegr8(void *that, uint8_t reg) {
    DBGLOG("gfxcon", "Regr8: reg: %x", reg);
    // turned on by using -lredregdbg
    return FunctionCast(wrapRegr8, callback->orgRegr8)(that, reg);
}

uint16_t GFXCon::wrapRegr16(void *that, uint16_t reg) {
    DBGLOG("gfxcon", "Regr16: reg: %x", reg);
    // turned on by using -lredregdbg
    return FunctionCast(wrapRegr16, callback->orgRegr16)(that, reg);
}

uint32_t GFXCon::wrapRegr32(void *that, uint32_t reg) {
    DBGLOG("gfxcon", "Regr32: reg: %x", reg);
    // turned on by using -lredregdbg
    return FunctionCast(wrapRegr32, callback->orgRegr32)(that, reg);
}

uint16_t GFXCon::wrapGetFamilyId(void) {
    auto id = FunctionCast(wrapGetFamilyId, callback->orgGetFamilyId)();
    DBGLOG("gfxcon", "getFamilyId >> %d", id);
    (void)id; // to get clang-analyze to shut up
    DBGLOG("gfxcon", "getFamilyId << %d", LRed::callback->currentFamilyId);
    return LRed::callback->currentFamilyId;
}

IOReturn GFXCon::wrapPopulateDeviceInfo(void *that) {
    DBGLOG("gfxcon", "populateDeviceInfo called!");
    auto ret = FunctionCast(wrapPopulateDeviceInfo, callback->orgPopulateDeviceInfo)(that);
    getMember<uint32_t>(that, 0x40) = LRed::callback->currentFamilyId;
    getMember<uint32_t>(that, 0x4C) = LRed::callback->currentEmulatedRevisionId;
    DBGLOG("gfxcon", "emulatedRevision == %x", LRed::callback->currentEmulatedRevisionId);
    return ret;
}

UInt32 GFXCon::wrapGetPllClock(void *that, uint8_t pll, void *clockparams) {
    DBGLOG("gfxcon", "getPllClock: pll: %x", pll);
    auto ret = FunctionCast(wrapGetPllClock, callback->orgGetPllClock)(that, pll, clockparams);
    DBGLOG("gfxcon", "getPllClock: returned %x", ret);
    return ret;
}
