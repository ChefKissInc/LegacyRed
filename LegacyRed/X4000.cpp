//!  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.5. See LICENSE for
//!  details.

#include "X4000.hpp"
#include "LRed.hpp"
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
        RouteRequestPlus requests[] = {
            {"__ZN36AMDRadeonX4000_AMDRadeonHWServicesCI16getMatchPropertyEv", forceX4000HWLibs},
            {"__ZN41AMDRadeonX4000_AMDRadeonHWServicesPolaris16getMatchPropertyEv", forceX4000HWLibs},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "HWServices",
            "Failed to route symbols");
    } else if (kextRadeonX4000.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();
        /** this is already really complicated, so to keep things breif:
         *  Carizzo uses UVD 6.0 and VCE 3.1, Stoney uses UVD 6.2 and VCE 3.4, both can only encode to H264 for whatever
         * reason, but Stoney can decode HEVC and so can Carrizo
         */
        auto stoney = (LRed::callback->chipType == ChipType::Stoney);
        auto carrizo = (LRed::callback->chipType == ChipType::Carrizo);

        UInt32 *orgChannelTypes = nullptr;
        mach_vm_address_t startHWEngines = 0;

        if (stoney) {
            SolveRequestPlus solveRequests[] = {
                {"__ZN28AMDRadeonX4000_AMDVIHardware32setupAndInitializeHWCapabilitiesEv",
                    this->orgSetupAndInitializeHWCapabilities},
                {"__ZZN37AMDRadeonX4000_AMDGraphicsAccelerator19createAccelChannelsEbE12channelTypes", orgChannelTypes},
                {"__ZN26AMDRadeonX4000_AMDHardware14startHWEnginesEv", startHWEngines},
            };
            PANIC_COND(!SolveRequestPlus::solveAll(patcher, index, solveRequests, address, size), "X4000",
                "Failed to resolve symbols for Stoney");
        } else if (carrizo) {
            SolveRequestPlus request {"__ZN28AMDRadeonX4000_AMDVIHardware32setupAndInitializeHWCapabilitiesEv",
                this->orgSetupAndInitializeHWCapabilities};
            PANIC_COND(!request.solve(patcher, index, address, size), "X4000", "Failed to solve HWCapabilities");
        } else {
            SolveRequestPlus request {"__ZN28AMDRadeonX4000_AMDCIHardware32setupAndInitializeHWCapabilitiesEv",
                this->orgSetupAndInitializeHWCapabilities};
            PANIC_COND(!request.solve(patcher, index, address, size), "X4000", "Failed to solve HWCapabilities");
        }

        if (stoney) {
            RouteRequestPlus requests[] = {
                {"__ZN35AMDRadeonX4000_AMDEllesmereHardware32setupAndInitializeHWCapabilitiesEv",
                    wrapSetupAndInitializeHWCapabilities},
                {"__ZN28AMDRadeonX4000_AMDVIHardware20initializeFamilyTypeEv", wrapInitializeFamilyType},
                {"__ZN26AMDRadeonX4000_AMDHardware12getHWChannelE20_eAMD_HW_ENGINE_TYPE18_eAMD_HW_RING_TYPE",
                    wrapGetHWChannel, this->orgGetHWChannel},
                {"__ZN32AMDRadeonX4000_AMDFijiPM4Channel27commitIndirectCommandBufferEP30AMD_SUBMIT_COMMAND_BUFFER_"
                 "INFO",
                    wrapCommitIndirectCommandBuffer, this->orgCommitindirectCommandBuffer},
                {"__ZN39AMDRadeonX4000_AMDFijiPM4ComputeChannel27commitIndirectCommandBufferEP30AMD_SUBMIT_COMMAND_"
                 "BUFFER_INFO",
                    wrapCommitIndirectCommandBufferCompute, this->orgCommitindirectCommandBufferCompute},
            };
            PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "X4000",
                "Failed to route symbols");
        } else if (carrizo) {
            RouteRequestPlus requests[] = {
                {"__ZN30AMDRadeonX4000_AMDFijiHardware32setupAndInitializeHWCapabilitiesEv",
                    wrapSetupAndInitializeHWCapabilities},
                {"__ZN28AMDRadeonX4000_AMDVIHardware20initializeFamilyTypeEv", wrapInitializeFamilyType},
                {"__ZN32AMDRadeonX4000_AMDFijiPM4Channel27commitIndirectCommandBufferEP30AMD_SUBMIT_COMMAND_BUFFER_"
                 "INFO",
                    wrapCommitIndirectCommandBuffer, this->orgCommitindirectCommandBuffer},
                {"__ZN39AMDRadeonX4000_AMDFijiPM4ComputeChannel27commitIndirectCommandBufferEP30AMD_SUBMIT_COMMAND_"
                 "BUFFER_INFO",
                    wrapCommitIndirectCommandBufferCompute, this->orgCommitindirectCommandBufferCompute},

            };
            PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "X4000",
                "Failed to route symbols");
        } else {
            RouteRequestPlus requests[] = {
                {"__ZN33AMDRadeonX4000_AMDBonaireHardware32setupAndInitializeHWCapabilitiesEv",
                    wrapSetupAndInitializeHWCapabilities},
                {"__ZN28AMDRadeonX4000_AMDCIHardware20initializeFamilyTypeEv", wrapInitializeFamilyType},
                {"__ZN29AMDRadeonX4000_AMDCIPM4Engine21initializeMicroEngineEv", wrapInitializeMicroEngine,
                    this->orgInitializeMicroEngine},
                {"__ZN29AMDRadeonX4000_AMDCIPM4Engine9softResetEjj", wrapSoftReset, this->orgSoftReset},
                {"__ZN28AMDRadeonX4000_AMDCIHardware16initializeVMRegsEv", wrapInitializeVMRegs,
                    this->orgInitializeVMRegs},
                {"__ZN30AMDRadeonX4000_AMDCIPM4Channel27commitIndirectCommandBufferEP30AMD_SUBMIT_COMMAND_BUFFER_INFO",
                    wrapCommitIndirectCommandBuffer, this->orgCommitindirectCommandBuffer},
                {"__ZN37AMDRadeonX4000_AMDCIPM4ComputeChannel27commitIndirectCommandBufferEP30AMD_SUBMIT_COMMAND_"
                 "BUFFER_INFO",
                    wrapCommitIndirectCommandBufferCompute, this->orgCommitindirectCommandBufferCompute},
            };
            PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "X4000",
                "Failed to route symbols");
        }

        RouteRequestPlus requests[] = {
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator5startEP9IOService", wrapAccelStart, orgAccelStart},
            {"__ZN26AMDRadeonX4000_AMDHardware17dumpASICHangStateEb.cold.1", wrapDumpASICHangState,
                this->orgDumpASICHangState},
            {"__ZN26AMDRadeonX4000_AMDHWMemory17adjustVRAMAddressEy", wrapAdjustVRAMAddress,
                this->orgAdjustVRAMAddress},
            {"__ZN27AMDRadeonX4000_AMDHWChannel18isDebugFlagEnabledEj", wrapIsDebugFlagEnabled},
            {"__ZN4Addr2V15CiLib19HwlInitGlobalParamsEPK18_ADDR_CREATE_INPUT", wrapHwlInitGlobalParams,
                orgHwlInitGlobalParams},
            {"__ZN35AMDRadeonX4000_AMDAccelVideoContext9getHWInfoEP13sHardwareInfo", wrapGetHWInfo, this->orgGetHWInfo},
            {"__ZN27AMDRadeonX4000_AMDHWChannel14waitForHwStampEj", wrapWaitForHwStamp, this->orgWaitForHwStamp},
        };
        PANIC_COND(!RouteRequestPlus::routeAll(patcher, index, requests, address, size), "X4000",
            "Failed to route symbols");

        if (stoney) {
            PANIC_COND(MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) != KERN_SUCCESS, "X4000",
                "Failed to enable kernel writing");
            /** TODO: Test this */
            orgChannelTypes[5] = 1;     //! Fix createAccelChannels so that it only starts SDMA0
            orgChannelTypes[11] = 0;    //! Fix getPagingChannel so that it gets SDMA0
            MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);

            LookupPatchPlus const allocHWEnginesPatch {&kextRadeonX4000, kAMDEllesmereHWallocHWEnginesOriginal,
                kAMDEllesmereHWallocHWEnginesPatched, 1};
            PANIC_COND(!allocHWEnginesPatch.apply(patcher, address, size), "X4000",
                "Failed to apply AllocateHWEngines patch: %d", patcher.getError());

            LookupPatchPlus const patch {&kextRadeonX4000, kStartHWEnginesOriginal, kStartHWEnginesMask,
                kStartHWEnginesPatched, kStartHWEnginesMask, 1};
            PANIC_COND(!patch.apply(patcher, startHWEngines, PAGE_SIZE), "X4000", "Failed to patch startHWEngines");
            DBGLOG("X4000", "Applied Singular SDMA lookup patch");
        }
        return true;
    }

    return false;
}

bool X4000::wrapAccelStart(void *that, IOService *provider) {
    DBGLOG("X4000", "accelStart: this = %p provider = %p", that, provider);
    callback->callbackAccelerator = that;
    auto ret = FunctionCast(wrapAccelStart, callback->orgAccelStart)(that, provider);
    DBGLOG("X4000", "accelStart returned %d", ret);
    return ret;
}

enum HWCapability : UInt64 {
    DisplayPipeCount = 0x04,    //! UInt32, unsure
    SECount = 0x34,             //! UInt32
    SHPerSE = 0x3C,             //! UInt32
    CUPerSH = 0x70,             //! UInt32
    Unknown0 = 0x8F,            //! bool
};

template<typename T>
static inline void setHWCapability(void *that, HWCapability capability, T value) {
    getMember<T>(that, (getKernelVersion() >= KernelVersion::Ventura ? 0x30 : 0x28) + capability) = value;
}

void X4000::wrapSetupAndInitializeHWCapabilities(void *that) {
    DBGLOG("X4000", "setupAndInitializeHWCapabilities: this = %p", that);
    UInt32 cuPerSh = 2;
    if (LRed::callback->chipType <= ChipType::Spooky || LRed::callback->chipType == ChipType::Carrizo) { cuPerSh = 8; }
    if (LRed::callback->isStoney3CU) { cuPerSh = 3; }
    setHWCapability<UInt32>(that, HWCapability::SECount, 1);
    setHWCapability<UInt32>(that, HWCapability::SHPerSE, 1);
    setHWCapability<UInt32>(that, HWCapability::CUPerSH, cuPerSh);
    FunctionCast(wrapSetupAndInitializeHWCapabilities, callback->orgSetupAndInitializeHWCapabilities)(that);

    if (LRed::callback->chipType == ChipType::Stoney) { setHWCapability<bool>(that, HWCapability::Unknown0, true); }
}

void X4000::wrapInitializeFamilyType(void *that) {
    DBGLOG("X4000", "initializeFamilyType << %x", LRed::callback->currentFamilyId);
    getMember<UInt32>(that, 0x308) = LRed::callback->currentFamilyId;
}

void *X4000::wrapGetHWChannel(void *that, UInt32 engineType, UInt32 ringId) {
    /** Redirect SDMA1 engine type to SDMA0 */
    return FunctionCast(wrapGetHWChannel, callback->orgGetHWChannel)(that, (engineType == 2) ? 1 : engineType, ringId);
}

const char *X4000::forceX4000HWLibs() {
    DBGLOG("HWServices", "Forcing HWServices to load X4000HWLibs");
    //! By default, X4000HWServices on CI loads X4050HWLibs, we override this here because X4050 has no KV logic.
    //! HWServicesFiji loads X4000HWLibs by default, so we don't need it there.
    //! Polaris is an interesting topic because it selects the name by using the Framebuffer name.
    return "Load4000";
}

void X4000::wrapDumpASICHangState(bool param1) {
    DBGLOG("X4000", "dumpASICHangState << (param1: %d)", param1);
    while (true) { IOSleep(36000000); }
}

UInt64 X4000::wrapAdjustVRAMAddress(void *that, UInt64 addr) {
    auto ret = FunctionCast(wrapAdjustVRAMAddress, callback->orgAdjustVRAMAddress)(that, addr);
    SYSLOG("X4000", "AdjustVRAMAddress: returned: 0x%llx, our value: 0x%llx", ret,
        ret != addr ? (ret + LRed::callback->fbOffset) : ret);
    return ret != addr ? (ret + LRed::callback->fbOffset) : ret;
}

UInt64 X4000::wrapInitializeMicroEngine(void *that) {
    DBGLOG("X4000", "initializeMicroEngine << (that: %p)", that);
    auto ret = FunctionCast(wrapInitializeMicroEngine, callback->orgInitializeMicroEngine)(that);
    DBGLOG("X4000", "initializeMicroEngine >> 0x%llX", ret);
    return ret;
}

bool X4000::wrapIsDebugFlagEnabled(void *that, UInt32 flag) {
    DBGLOG("X4000", "isDebugFlagEnabled << flag: 0x%x", flag);
    DBGLOG("X4000", "isDebugFlagEnabled >>");
    return false;
}

void X4000::wrapInitializeVMRegs(void *that) {
    FunctionCast(wrapInitializeVMRegs, callback->orgInitializeVMRegs)(that);
    if (LRed::callback->chipType <= ChipType::Spooky) {
        UInt32 tmp = LRed::callback->readReg32(mmCHUB_CONTROL);
        tmp &= ~bypassVM;
        LRed::callback->writeReg32(mmCHUB_CONTROL, tmp);
    }
}

void X4000::wrapSoftReset(void *that, UInt32 addr, UInt32 val) {
    DBGLOG("X4000", "PM4 softReset: 0x%x, 0x%x", addr, val);
    FunctionCast(wrapSoftReset, callback->orgSoftReset)(that, addr, val);
    DBGLOG("X4000", "PM4 softReset >> void");
}

UInt32 X4000::wrapCommitIndirectCommandBuffer(void *that, void *cmdbuf) {
    DBGLOG("X4000", "PM4: commitIndirectCommandBuffer: cmdbuf: off 0x0: 0x%x, off 0x4: 0x%x, off 0xC: 0x%x",
        getMember<UInt32>(cmdbuf, 0), getMember<UInt32>(cmdbuf, 0x4), getMember<UInt32>(cmdbuf, 0xC));
    auto ret = FunctionCast(wrapCommitIndirectCommandBuffer, callback->orgCommitindirectCommandBuffer)(that, cmdbuf);
    DBGLOG("X4000", "PM4: commitIndirectCommandBuffer >> 0x%x", ret);
    return ret;
}

UInt32 X4000::wrapCommitIndirectCommandBufferCompute(void *that, void *cmdbuf) {
    DBGLOG("X4000", "PM4 COMPUTE: commitIndirectCommandBuffer: cmdbuf: off 0x0: 0x%x, off 0x4: 0x%x, off 0xC: 0x%x",
        getMember<UInt32>(cmdbuf, 0), getMember<UInt32>(cmdbuf, 0x4), getMember<UInt32>(cmdbuf, 0xC));
    auto ret = FunctionCast(wrapCommitIndirectCommandBufferCompute,
        callback->orgCommitindirectCommandBufferCompute)(that, cmdbuf);
    DBGLOG("X4000", "PM4 COMPUTE: commitIndirectCommandBuffer >> 0x%x", ret);
    return ret;
}

int X4000::wrapHwlInitGlobalParams(void *that, const void *creationInfo) {
    DBGLOG("X4000", "HwlInitGlobalParams >>");
    auto ret = FunctionCast(wrapHwlInitGlobalParams, callback->orgHwlInitGlobalParams)(that, creationInfo);
    UInt32 pipes = getMember<UInt32>(that, 0x38);
    if (pipes != 2 || (pipes != 4 && LRed::callback->chipType == ChipType::Spectre)) {
        DBGLOG("X4000", "Correcting pipe number");
        if (LRed::callback->chipType == ChipType::Spectre) {
            getMember<UInt32>(that, 0x38) = 4;
        } else {
            getMember<UInt32>(that, 0x38) = 2;
        }
    }
    DBGLOG("X4000", "HwlInitGlobalParams << 0x%x, pipes: %d", ret, pipes);
    return ret;
}

IOReturn X4000::wrapGetHWInfo(void *ctx, void *hwInfo) {
    auto ret = FunctionCast(wrapGetHWInfo, callback->orgGetHWInfo)(ctx, hwInfo);
    getMember<UInt32>(hwInfo, 0x4) = LRed::callback->isGCN3 ? (LRed::callback->isStoney ? 0x67DF : 0x7300) : 0x6640;
    return ret;
}

bool X4000::wrapWaitForHwStamp(void *accelChannel, UInt32 stamp) {
    auto ret = FunctionCast(wrapWaitForHwStamp, callback->orgWaitForHwStamp)(accelChannel, stamp);
    size_t retries = 0;
    while (!ret) {
        if (retries == 0x5) {
            DBGLOG("X4000", "well crap, waiting didn't work!");
            break;
        }
        DBGLOG("X4000", "WAIT. LONGER.");
        ret = FunctionCast(wrapWaitForHwStamp, callback->orgWaitForHwStamp)(accelChannel, stamp);
        retries++;
    }
    DBGLOG("X4000", "waitForHwStamp >> 0x%x", ret);
    return ret;
}
