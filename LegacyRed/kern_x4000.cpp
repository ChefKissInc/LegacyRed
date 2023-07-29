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
        auto isStoney = (LRed::callback->chipType == ChipType::Stoney);
        RouteRequestPlus requests[] = {
            {"__ZN36AMDRadeonX4000_AMDRadeonHWServicesCI16getMatchPropertyEv", forceX4000HWLibs, !useGcn3Logic},
            {"__ZN41AMDRadeonX4000_AMDRadeonHWServicesPolaris16getMatchPropertyEv", forceX4000HWLibs, isStoney},
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
        auto isStoney = (LRed::callback->chipType == ChipType::Stoney);

        uint32_t *orgChannelTypes = nullptr;
        mach_vm_address_t startHWEngines = 0;

        SolveRequestPlus solveRequests[] = {
            {"__ZN31AMDRadeonX4000_AMDBaffinPM4EngineC1Ev", this->orgBaffinPM4EngineConstructor, isStoney},
            {"__ZN30AMDRadeonX4000_AMDVIsDMAEngineC1Ev", this->orgGFX8SDMAEngineConstructor, isStoney},
            {"__ZN32AMDRadeonX4000_AMDUVD6v3HWEngineC1Ev", this->orgPolarisUVDEngineConstructor, isStoney},
            {"__ZN30AMDRadeonX4000_AMDVISAMUEngineC1Ev", this->orgGFX8SAMUEngineConstructor, isStoney},
            {"__ZN32AMDRadeonX4000_AMDVCE3v4HWEngineC1Ev", this->orgPolarisVCEEngineConstructor, isStoney},
            {"__ZN28AMDRadeonX4000_AMDCIHardware32setupAndInitializeHWCapabilitiesEv",
                this->orgSetupAndInitializeHWCapabilities, !useGcn3Logic},
            {"__ZN28AMDRadeonX4000_AMDVIHardware32setupAndInitializeHWCapabilitiesEv",
                this->orgSetupAndInitializeHWCapabilities, useGcn3Logic},
            {"__ZZN37AMDRadeonX4000_AMDGraphicsAccelerator19createAccelChannelsEbE12channelTypes", orgChannelTypes,
                isStoney},
            {"__ZN26AMDRadeonX4000_AMDHardware14startHWEnginesEv", startHWEngines},
        };
        PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "x4000",
            "Failed to resolve symbols");

        RouteRequestPlus requests[] = {
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator5startEP9IOService", wrapAccelStart, orgAccelStart},
            {"__ZN33AMDRadeonX4000_AMDBonaireHardware32setupAndInitializeHWCapabilitiesEv",
                wrapSetupAndInitializeHWCapabilities, !useGcn3Logic},
            {"__ZN30AMDRadeonX4000_AMDFijiHardware32setupAndInitializeHWCapabilitiesEv",
                wrapSetupAndInitializeHWCapabilities, (useGcn3Logic && !isStoney)},
            {"__ZN35AMDRadeonX4000_AMDEllesmereHardware32setupAndInitializeHWCapabilitiesEv",
                wrapSetupAndInitializeHWCapabilities, isStoney},
            {"__ZN28AMDRadeonX4000_AMDCIHardware20initializeFamilyTypeEv", wrapInitializeFamilyType, !useGcn3Logic},
            {"__ZN28AMDRadeonX4000_AMDVIHardware20initializeFamilyTypeEv", wrapInitializeFamilyType, useGcn3Logic},
            {"__ZN26AMDRadeonX4000_AMDHardware12getHWChannelE20_eAMD_HW_ENGINE_TYPE18_eAMD_HW_RING_TYPE",
                wrapGetHWChannel, this->orgGetHWChannel, isStoney},
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

        if (isStoney) {
            PANIC_COND(MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) != KERN_SUCCESS, "x4000",
                "Failed to enable kernel writing");
            /** TODO: Test this */
            orgChannelTypes[5] = 1;     // Fix createAccelChannels so that it only starts SDMA0
            orgChannelTypes[11] = 0;    // Fix getPagingChannel so that it gets SDMA0
            MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);

            LookupPatchPlus const allocHWEnginesPatch {&kextRadeonX4000, kAMDEllesmereHWallocHWEnginesOriginal,
                kAMDEllesmereHWallocHWEnginesPatched, 1, isStoney};
            PANIC_COND(!allocHWEnginesPatch.apply(patcher, slide, size), "x4000",
                "Failed to apply AllocateHWEngines patch: %d", patcher.getError());

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
    DisplayPipeCount = 0x04,    // uint32_t // unsure
    SECount = 0x24,             // uint32_t
    SHPerSE = 0x2C,             // uint32_t
    CUPerSH = 0x60,             // uint32_t
};

template<typename T>
static inline void setHWCapability(void *that, HWCapability capability, T value) {
    getMember<T>(that, (getKernelVersion() >= KernelVersion::Ventura ? 0x30 : 0x28) + capability) = value;
}

void X4000::wrapSetupAndInitializeHWCapabilities(void *that) {
    DBGLOG("x4000", "setupAndInitializeHWCapabilities: this = %p", that);
    switch (LRed::callback->chipType) {
        case ChipType::Spectre:
            [[fallthrough]];
        case ChipType::Spooky: {
            setHWCapability<uint32_t>(that, HWCapability::SECount, 1);
            setHWCapability<uint32_t>(that, HWCapability::SHPerSE, 1);
            switch (LRed::callback->deviceId) {
                case 0x1304:
                    [[fallthrough]];
                case 0x1305:
                    [[fallthrough]];
                case 0x130C:
                    [[fallthrough]];
                case 0x130F:
                    [[fallthrough]];
                case 0x1310:
                    [[fallthrough]];
                case 0x1311:
                    [[fallthrough]];
                case 0x131C:
                    setHWCapability<uint32_t>(that, HWCapability::CUPerSH, 8);
                    break;
                case 0x1309:
                    [[fallthrough]];
                case 0x130A:
                    [[fallthrough]];
                case 0x130D:
                    [[fallthrough]];
                case 0x1313:
                    [[fallthrough]];
                case 0x131D:
                    setHWCapability<uint32_t>(that, HWCapability::CUPerSH, 6);
                    break;
                case 0x1306:
                    [[fallthrough]];
                case 0x1307:
                    [[fallthrough]];
                case 0x130B:
                    [[fallthrough]];
                case 0x130E:
                    [[fallthrough]];
                case 0x1315:
                    [[fallthrough]];
                case 0x131B:
                    setHWCapability<uint32_t>(that, HWCapability::CUPerSH, 4);
                    break;
                default:
                    setHWCapability<uint32_t>(that, HWCapability::CUPerSH, 3);
                    break;
            }
            break;
        }
        case ChipType::Kalindi:
            [[fallthrough]];
        case ChipType::Godavari: {
            setHWCapability<uint32_t>(that, HWCapability::SECount, 1);
            setHWCapability<uint32_t>(that, HWCapability::SHPerSE, 1);
            setHWCapability<uint32_t>(that, HWCapability::CUPerSH, 2);
            break;
        }
        case ChipType::Carrizo: {
            setHWCapability<uint32_t>(that, HWCapability::SECount, 1);
            setHWCapability<uint32_t>(that, HWCapability::SHPerSE, 1);
            switch (LRed::callback->revision) {
                case 0xC4:
                    [[fallthrough]];
                case 0x84:
                    [[fallthrough]];
                case 0xC8:
                    [[fallthrough]];
                case 0xCC:
                    [[fallthrough]];
                case 0xE1:
                    [[fallthrough]];
                case 0xE3:
                    setHWCapability<uint32_t>(that, HWCapability::CUPerSH, 8);
                    break;
                case 0xC5:
                    [[fallthrough]];
                case 0x81:
                    [[fallthrough]];
                case 0x85:
                    [[fallthrough]];
                case 0xC9:
                    [[fallthrough]];
                case 0xCD:
                    [[fallthrough]];
                case 0xE2:
                    [[fallthrough]];
                case 0xE4:
                    [[fallthrough]];
                case 0xC6:
                    [[fallthrough]];
                case 0xCA:
                    [[fallthrough]];
                case 0xCE:
                    [[fallthrough]];
                case 0x88:
                    setHWCapability<uint32_t>(that, HWCapability::CUPerSH, 6);
                    break;
                default:
                    setHWCapability<uint32_t>(that, HWCapability::CUPerSH, 4);
                    break;
            }
            break;
        }
        case ChipType::Stoney: {
            setHWCapability<uint32_t>(that, HWCapability::SECount, 1);
            setHWCapability<uint32_t>(that, HWCapability::SHPerSE, 1);
            switch (LRed::callback->chipVariant) {
                case ChipVariant::s3CU: {
                    setHWCapability<uint32_t>(that, HWCapability::CUPerSH, 3);
                    break;
                }
                case ChipVariant::s2CU: {
                    setHWCapability<uint32_t>(that, HWCapability::CUPerSH, 2);
                    break;
                }
                default: {
                    PANIC("x4000", "Using Stoney ChipType on unlabelled ChipVariant!");
                    break;
                }
            }
            break;
        }
        default: {
            PANIC("x4000", "Unknown ChipType!");
            break;
        }
    }
    FunctionCast(wrapSetupAndInitializeHWCapabilities, callback->orgSetupAndInitializeHWCapabilities)(that);
}

void X4000::wrapInitializeFamilyType(void *that) {
    DBGLOG("x4000", "initializeFamilyType << %x", LRed::callback->currentFamilyId);
    getMember<uint32_t>(that, 0x2F8) = LRed::callback->currentFamilyId;
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
    // By default, X4000HWServices on CI loads X4050HWLibs, we override this here
    return "Load4000";
}
