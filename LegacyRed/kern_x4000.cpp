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
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "hwservices",
            "Failed to route symbols");
    } else if (kextRadeonX4000.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();
        /** this is already really complicated, so to keep things breif:
         *  Carizzo uses UVD 6.0 and VCE 3.1, Stoney uses UVD 6.2 and VCE 3.4, both can only encode to H264 for whatever
         * reason, but Stoney can decode HEVC and so can Carrizo
         */
        bool useGcn3Logic = LRed::callback->isGCN3;
        bool useGcn4AndPatchLogic = (LRed::callback->chipType == ChipType::Stoney);

        uint32_t *orgChannelTypes = nullptr;
        mach_vm_address_t startHWEngines = 0;

        SolveRequestPlus solveRequests[] = {
            {"__ZN31AMDRadeonX4000_AMDBaffinPM4EngineC1Ev", this->orgBaffinPM4EngineConstructor, useGcn4AndPatchLogic},
            {"__ZN30AMDRadeonX4000_AMDVIsDMAEngineC1Ev", this->orgGFX8SDMAEngineConstructor, useGcn4AndPatchLogic},
            {"__ZN32AMDRadeonX4000_AMDUVD6v3HWEngineC1Ev", this->orgPolarisUVDEngineConstructor, useGcn4AndPatchLogic},
            {"__ZN30AMDRadeonX4000_AMDVISAMUEngineC1Ev", this->orgGFX8SAMUEngineConstructor, useGcn4AndPatchLogic},
            {"__ZN32AMDRadeonX4000_AMDVCE3v4HWEngineC1Ev", this->orgPolarisVCEEngineConstructor, useGcn4AndPatchLogic},
            {"__ZN28AMDRadeonX4000_AMDCIHardware32setupAndInitializeHWCapabilitiesEv",
                this->orgSetupAndInitializeHWCapabilities, !useGcn3Logic},
            {"__ZN28AMDRadeonX4000_AMDVIHardware32setupAndInitializeHWCapabilitiesEv",
                this->orgSetupAndInitializeHWCapabilities, useGcn3Logic},
            {"__ZZN37AMDRadeonX4000_AMDGraphicsAccelerator19createAccelChannelsEbE12channelTypes", orgChannelTypes,
                useGcn4AndPatchLogic},
            {"__ZN26AMDRadeonX4000_AMDHardware14startHWEnginesEv", startHWEngines},
        };
        PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "x4000",
            "Failed to resolve symbols");

        RouteRequestPlus requests[] = {
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator5startEP9IOService", wrapAccelStart, orgAccelStart},
            {"__ZN35AMDRadeonX4000_AMDEllesmereHardware17allocateHWEnginesEv", wrapAllocateHWEngines,
                useGcn4AndPatchLogic},
            {"__ZN33AMDRadeonX4000_AMDBonaireHardware32setupAndInitializeHWCapabilitiesEv",
                wrapSetupAndInitializeHWCapabilities, !useGcn3Logic},
            {"__ZN31AMDRadeonX4000_AMDFijiHardware32setupAndInitializeHWCapabilitiesEv",
                wrapSetupAndInitializeHWCapabilities, (useGcn3Logic && !useGcn4AndPatchLogic)},
            {"__ZN35AMDRadeonX4000_AMDEllesmereHardware32setupAndInitializeHWCapabilitiesEv",
                wrapSetupAndInitializeHWCapabilities, useGcn4AndPatchLogic},
            {"__ZN28AMDRadeonX4000_AMDCIHardware20initializeFamilyTypeEv", wrapInitializeFamilyType, !useGcn3Logic},
            {"__ZN28AMDRadeonX4000_AMDVIHardware20initializeFamilyTypeEv", wrapInitializeFamilyType, useGcn3Logic},
            {"__ZN26AMDRadeonX4000_AMDHardware12getHWChannelE20_eAMD_HW_ENGINE_TYPE18_eAMD_HW_RING_TYPE",
                wrapGetHWChannel, this->orgGetHWChannel, LRed::callback->chipType == ChipType::Stoney},
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator15configureDeviceEP11IOPCIDevice", wrapConfigureDevice,
                this->orgConfigureDevice},
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator14initLinkToPeerEPKc", wrapInitLinkToPeer,
                this->orgInitLinkToPeer},
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator15createHWHandlerEv", wrapCreateHWHandler,
                this->orgCreateHWHandler},
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator17createHWInterfaceEP11IOPCIDevice", wrapCreateHWInterface,
                this->orgCreateHWInterface},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "x4000",
            "Failed to route symbols");

        if (useGcn4AndPatchLogic) {
            PANIC_COND(MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) != KERN_SUCCESS, "x4000",
                "Failed to enable kernel writing");
            /** TODO: Test this */
            orgChannelTypes[5] = 1;     // Fix createAccelChannels so that it only starts SDMA0
            orgChannelTypes[11] = 0;    // Fix getPagingChannel so that it gets SDMA0
            MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);

            LookupPatchPlus const patch {&kextRadeonX4000, kStartHWEnginesOriginal, kStartHWEnginesMask,
                kStartHWEnginesPatched, kStartHWEnginesMask, 1};
            PANIC_COND(!patch.apply(&patcher, startHWEngines, PAGE_SIZE), "x4000", "Failed to patch startHWEngines");
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
        auto catalina = getKernelVersion() == KernelVersion::Catalina;
        auto fieldBase = catalina ? 0x340 : 0x3B0;

        auto *pm4 = OSObject::operator new(0x198);
        callback->orgBaffinPM4EngineConstructor(pm4);
        getMember<void *>(that, fieldBase) = pm4;

        auto *sdma = OSObject::operator new(0x100);
        callback->orgGFX8SDMAEngineConstructor(sdma);
        getMember<void *>(that, fieldBase + 0x8) = sdma;

        auto *uvd = OSObject::operator new(0x2F0);
        callback->orgPolarisUVDEngineConstructor(uvd);
        getMember<void *>(that, fieldBase + catalina ? 0x18 : 0x28) = uvd;

        // I swear to god, this one infuriates me to no end, I love ghidra.
        auto *samu = OSObject::operator new(0x1D0);
        callback->orgGFX8SAMUEngineConstructor(samu);
        getMember<void *>(that, fieldBase + catalina ? 0x40 : 0x50) = samu;

        auto *vce = OSObject::operator new(0x258);
        callback->orgPolarisVCEEngineConstructor(vce);
        getMember<void *>(that, fieldBase + catalina ? 0x28 : 0x38) = vce;

    } else {
        PANIC("x4000", "Using Polaris/VI logic on unsupported ASIC!");
    }
    DBGLOG("x4000", "Finished AllocateHWEngines wrap.");
    return true;
}

void *X4000::wrapGetHWChannel(void *that, uint32_t engineType, uint32_t ringId) {
    /** Redirect SDMA1 engine type to SDMA0 */
    return FunctionCast(wrapGetHWChannel, callback->orgGetHWChannel)(that, (engineType == 2) ? 1 : engineType, ringId);
}

uint64_t X4000::wrapCreateHWInterface(void *that, IOPCIDevice *dev) {
    DBGLOG("x4000", "createHWInterface called!");
    auto ret = FunctionCast(wrapCreateHWInterface, callback->orgCreateHWInterface)(that, dev);
    DBGLOG("x4000", "createHWInterface returned 0x%x", ret);
    return ret;
}

uint64_t X4000::wrapCreateHWHandler(void *that) {
    DBGLOG("x4000", "createHWHandler called!");
    auto ret = FunctionCast(wrapCreateHWHandler, callback->orgCreateHWHandler)(that);
    DBGLOG("x4000", "createHWHandler returned 0x%x", ret);
    return ret;
}

uint64_t X4000::wrapConfigureDevice(void *that, IOPCIDevice *dev) {
    DBGLOG("x4000", "createHWInterface called!");
    auto ret = FunctionCast(wrapCreateHWInterface, callback->orgCreateHWInterface)(that, dev);
    DBGLOG("x4000", "createHWInterface returned 0x%x", ret);
    return ret;
}

IOService *X4000::wrapInitLinkToPeer(void *that, const char *matchCategoryName) {
    DBGLOG("x4000", "initLinkToPeer called!");
    auto ret = FunctionCast(wrapInitLinkToPeer, callback->orgInitLinkToPeer)(that, matchCategoryName);
    DBGLOG("x4000", "initLinkToPeer returned 0x%x", ret);
    return ret;
}

char *X4000::forceX4000HWLibs() {
    DBGLOG("hwservices", "Forcing HWServices to load X4000HWLibs");
    return "Load4000";
}
