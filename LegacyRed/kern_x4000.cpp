//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_x4000.hpp"
#include "kern_lred.hpp"
#include "kern_patches.hpp"
#include <Headers/kern_api.hpp>

static const char *pathRadeonX4000 = "/System/Library/Extensions/AMDRadeonX4000.kext/Contents/MacOS/AMDRadeonX4000";

static const char *pathRadeonX4000HWServices =
    "/System/Library/Extensions/AMDRadeonX4000HWServices.kext/Contents/MacOS/AMDRadeonX4000HWServices";

static KernelPatcher::KextInfo kextRadeonX4000 {"com.apple.kext.AMDRadeonX4000", &pathRadeonX4000, 1, {}, {},
    KernelPatcher::KextInfo::Unloaded};

static KernelPatcher::KextInfo kextRadeonX4000HWServices {"com.apple.kext.AMDRadeonX4000HWServices",
    &pathRadeonX4000HWServices, 1, {}, {}, KernelPatcher::KextInfo::Unloaded};

X4000 *X4000::callback = nullptr;

void X4000::init() {
    callback = this;
    lilu.onKextLoadForce(&kextRadeonX4000);
	lilu.onKextLoadForce(&kextRadeonX4000HWServices);
}

bool X4000::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextRadeonX4000HWServices.loadIndex == index) {
        bool useGcn3Logic = LRed::callback->isGCN3;
        RouteRequestPlus requests[] = {
            {"__ZN36AMDRadeonX4000_AMDRadeonHWServicesCI16getMatchPropertyEv", forceX4000HWLibs, !useGcn3Logic},
            {"__ZN36AMDRadeonX4000_AMDRadeonHWServicesVI16getMatchPropertyEv", forceX4000HWLibs, useGcn3Logic},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "hwservices",
            "Failed to route symbols");
    } else if (kextRadeonX4000.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();
        bool useGcn3Logic = LRed::callback->isGCN3;

        uint32_t *orgChannelTypes = nullptr;

        SolveRequestPlus solveRequests[] = {
            {"__ZN29AMDRadeonX4000_AMDVIPM4EngineC1Ev", this->orgGFX8PM4EngineConstructor, useGcn3Logic},
            {"__ZN30AMDRadeonX4000_AMDVIsDMAEngineC1Ev", this->orgGFX8SDMAEngineConstructor, useGcn3Logic},
            {"__ZN31AMDRadeonX4000_AMDVIUVDHWEngineC1Ev", this->orgGFX8UVDEngineConstructor, useGcn3Logic},
            {"__ZN30AMDRadeonX4000_AMDVISAMUEngineC1Ev", this->orgGFX8SAMUEngineConstructor, useGcn3Logic},
            {"__ZN31AMDRadeonX4000_AMDVIVCEHWEngineC1Ev", this->orgGFX8VCEEngineConstructor, useGcn3Logic},
            {"__ZZN37AMDRadeonX4000_AMDGraphicsAccelerator19createAccelChannelsEbE12channelTypes", orgChannelTypes,
                LRed::callback->chipType == ChipType::Stoney},
            {"__ZN28AMDRadeonX4000_AMDCIHardware32setupAndInitializeHWCapabilitiesEv",
                this->orgSetupAndInitializeHWCapabilities, !useGcn3Logic},
            {"__ZN28AMDRadeonX4000_AMDVIHardware32setupAndInitializeHWCapabilitiesEv",
                this->orgSetupAndInitializeHWCapabilities, useGcn3Logic},
            {"__ZN26AMDRadeonX4000_AMDHardware12getHWChannelE20_eAMD_HW_ENGINE_TYPE18_eAMD_HW_RING_TYPE",
                orgGetHWChannel, LRed::callback->chipType == ChipType::Stoney},
        };
        PANIC_COND(SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "x4000",
            "Failed to resolve symbols");

        RouteRequestPlus requests[] = {
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator5startEP9IOService", wrapAccelStart, orgAccelStart},
            {"__ZN28AMDRadeonX4000_AMDVIHardware17allocateHWEnginesEv", wrapAllocateHWEngines, useGcn3Logic},
            {"__ZN28AMDRadeonX4000_AMDCIHardware32setupAndInitializeHWCapabilitiesEv",
                wrapSetupAndInitializeHWCapabilities, !useGcn3Logic},
            {"__ZN28AMDRadeonX4000_AMDVIHardware32setupAndInitializeHWCapabilitiesEv",
                wrapSetupAndInitializeHWCapabilities, useGcn3Logic},
            {"__ZN28AMDRadeonX4000_AMDCIHardware20initializeFamilyTypeEv", wrapInitializeFamilyType, !useGcn3Logic},
            {"__ZN28AMDRadeonX4000_AMDVIHardware20initializeFamilyTypeEv", wrapInitializeFamilyType, useGcn3Logic},
            {"__ZN26AMDRadeonX4000_AMDHardware12getHWChannelE20_eAMD_HW_ENGINE_TYPE18_eAMD_HW_RING_TYPE",
                wrapGetHWChannel, LRed::callback->chipType == ChipType::Stoney},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "x4000",
            "Failed to route symbols");

        // PANIC_COND(MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) != KERN_SUCCESS, "x4000",
        //  "Failed to enable kernel writing");
        /** TODO: Port this */
        // orgChannelTypes[5] = 1;     // Fix createAccelChannels so that it only starts SDMA0
        // orgChannelTypes[11] = 0;    // Fix getPagingChannel so that it gets SDMA0
        // MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);

        if (LRed::callback->chipType == ChipType::Stoney) {
            KernelPatcher::LookupPatch patch = {&kextRadeonX4000, kStartHWEnginesOriginal, kStartHWEnginesPatched,
                arrsize(kStartHWEnginesOriginal), 1};
            patcher.applyLookupPatch(&patch);
            patcher.clearError();
            DBGLOG("x4000", "Applied Singular SDMA lookup patch");
        }
        return true;
    }

    return false;
}

bool X4000::wrapAccelStart(void *that, IOService *provider) {
    DBGLOG("x4000", "accelStart: this = %p provider = %p", that, provider);
    callback->callbackAccelerator = that;
    auto ret = FunctionCast(wrapAccelStart, callback->orgAccelStart)(that, provider);
    DBGLOG("x4000", "accelStart returned %d", ret);
    return ret;
}

// Likely to be unused, here for incase we need to use it for X4000::setupAndInitializeHWCapabilities
enum HWCapability : uint64_t {
    DisplayPipeCount = 0x04,    // uint32_t
    SECount = 0x34,             // uint32_t
    SHPerSE = 0x3C,             // uint32_t
    CUPerSH = 0x70,             // uint32_t
    HasUVD0 = 0x84,             // bool
    HasUVD1 = 0x85,             // bool
    HasVCE = 0x86,              // bool
    HasVCN0 = 0x87,             // bool
    HasVCN1 = 0x88,             // bool
    HasHDCP = 0x8D,             // bool
    HasSDMAPageQueue = 0x98,    // bool
};

void X4000::wrapSetupAndInitializeHWCapabilities(void *that) {
    DBGLOG("x4000", "setupAndInitializeHWCapabilities: this = %p", that);
    FunctionCast(wrapSetupAndInitializeHWCapabilities, callback->orgSetupAndInitializeHWCapabilities)(that);
}

void X4000::wrapInitializeFamilyType(void *that) {
    getMember<uint32_t>(that, 0x308) = LRed::callback->isGCN3 ? AMDGPU_FAMILY_CZ : AMDGPU_FAMILY_KV;
}

/** Rough calculations based on AMDRadeonX4000's Assembly  */

bool X4000::wrapAllocateHWEngines(void *that) {
    DBGLOG("x4000", "Wrap for AllocateHWEngines starting...");
    if (LRed::callback->isGCN3) {
        // since the AMDRadeonXY000 code sucks, older versions use a wrap around OSObject::operator new, and since we
        // want to maximise compatibility, we use OSObject here rather than an IOMallocZero([engine value]).

        auto *pm4 = OSObject::operator new(0x198);
        callback->orgGFX8PM4EngineConstructor(pm4);
        getMember<void *>(that, 0x3B0) = pm4;

        auto *sdma0 = OSObject::operator new(0x100);
        callback->orgGFX8SDMAEngineConstructor(sdma0);
        getMember<void *>(that, 0x3B8) = sdma0;

        // Only one SDMA engine is present on Stoney APUs

        if (LRed::callback->chipType == ChipType::Stoney) {
            DBGLOG("x4000", "Using only 1 SDMA Engine for Stoney.");
        } else {
            auto *sdma1 = OSObject::operator new(0x100);
            callback->orgGFX8SDMAEngineConstructor(getMember<void *>(that, 0x3C0) = sdma1);
            getMember<void *>(that, 0x3C0) = sdma1;
        }

        auto *uvd = OSObject::operator new(0x2F0);
        callback->orgGFX8UVDEngineConstructor(uvd);
        getMember<void *>(that, 0x3D8) = uvd;

        // I swear to god, this one infuriates me to no end, I love ghidra.
        auto *samu = OSObject::operator new(0x1D0);
        callback->orgGFX8SAMUEngineConstructor(samu);
        getMember<void *>(that, 0x400) = samu;

        auto *vce = OSObject::operator new(0x258);
        callback->orgGFX8VCEEngineConstructor(vce);
        getMember<void *>(that, 0x3E8) = vce;

        // is it even worth it to have these just for pre-Catalina support? lol
    } else {
        PANIC("x4000", "Using VI logic on unsupported ASIC!");
    }
    DBGLOG("x4000", "Finished AllocateHWEngines wrap.");
    return true;
}

void *X4000::wrapGetHWChannel(void *that, uint32_t engineType, uint32_t ringId) {
    /** Redirect SDMA1 engine type to SDMA0 */
    return FunctionCast(wrapGetHWChannel, callback->orgGetHWChannel)(that, (engineType == 2) ? 1 : engineType, ringId);
}

char *X4000::forceX4000HWLibs() { return "Load4000"; }
