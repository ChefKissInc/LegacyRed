//  Copyright © 2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

//  Copyright © 2023 Zormeister. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_lred.hpp"
#include "kern_amd.hpp"
#include "kern_model.hpp"
#include <Headers/kern_api.hpp>
#include <Headers/kern_devinfo.hpp>

static const char *pathRadeonX4000HWLibs = "/System/Library/Extensions/AMDRadeonX4000HWServices.kext/Contents/PlugIns/"
                                           "AMDRadeonX4000HWLibs.kext/Contents/MacOS/AMDRadeonX4050HWLibs";
static const char *pathRadeonFramebuffer =
    "/System/Library/Extensions/AMDFramebuffer.kext/Contents/MacOS/AMDFramebuffer";

static const char *pathRadeonX4000 = "/System/Library/Extensions/AMDRadeonX4000.kext/Contents/MacOS/AMDRadeonX4000";

static const char *pathAMD8000Controller =
    "/System/Library/Extensions/AMD8000Controller.kext/Contents/MacOS/AMD8000Controller";

static const char *pathRadeonSupport = "/System/Library/Extensions/AMDSupport.kext/Contents/MacOS/AMDSupport";

static const char *pathAMD9000Controller =
    "/System/Library/Extensions/AMD9000Controller.kext/Contents/MacOS/AMD9000Controller";

static KernelPatcher::KextInfo kextRadeonX4000HWLibs {"com.apple.kext.AMDRadeonX4050HWLibs", &pathRadeonX4000HWLibs, 1,
    {}, {}, KernelPatcher::KextInfo::Unloaded};

static KernelPatcher::KextInfo kextRadeonFramebuffer {"com.apple.kext.AMDFramebuffer", &pathRadeonFramebuffer, 1, {},
    {}, KernelPatcher::KextInfo::Unloaded};

static KernelPatcher::KextInfo kextAMD8000Controller = {"com.apple.kext.AMD8000Controller", &pathAMD8000Controller, 1,
    {}, {}, KernelPatcher::KextInfo::Unloaded};

static KernelPatcher::KextInfo kextAMD9000Controller = {"com.apple.kext.AMD9000Controller", &pathAMD9000Controller, 1,
    {}, {}, KernelPatcher::KextInfo::Unloaded};

static KernelPatcher::KextInfo kextRadeonX4000 {"com.apple.kext.AMDRadeonX4000", &pathRadeonX4000, 1, {}, {},
    KernelPatcher::KextInfo::Unloaded};

LRed *LRed::callbackLRed = nullptr;

void LRed::init() {
    SYSLOG("lred", "Please don't support tonymacx86.com!");
    callbackLRed = this;

    lilu.onPatcherLoadForce(
        [](void *user, KernelPatcher &patcher) { static_cast<LRed *>(user)->processPatcher(patcher); }, this);
    lilu.onKextLoadForce(&kextRadeonX4000HWLibs);
    lilu.onKextLoadForce(&kextRadeonFramebuffer);
    lilu.onKextLoad(&kextAMD8000Controller);
    lilu.onKextLoad(&kextAMD9000Controller);
    lilu.onKextLoadForce(&kextRadeonX4000);
    lilu.onKextLoadForce(    // For compatibility
        nullptr, 0,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            static_cast<LRed *>(user)->processKext(patcher, index, address, size);
        },
        this);
}

void LRed::deinit() {
    if (this->vbiosData) { this->vbiosData->release(); }
}

void LRed::processPatcher(KernelPatcher &patcher) {
    KernelPatcher::RouteRequest requests[] = {
        {"__ZN15OSMetaClassBase12safeMetaCastEPKS_PK11OSMetaClass", wrapSafeMetaCast, orgSafeMetaCast},
    };
    PANIC_COND(!patcher.routeMultiple(KernelPatcher::KernelID, requests), MODULE_SHORT,
        "Failed to route OSMetaClassBase::safeMetaCast");

    auto *devInfo = DeviceInfo::create();
    if (!devInfo) {
        SYSLOG(MODULE_SHORT, "Failed to create DeviceInfo");
        return;
    }

    devInfo->processSwitchOff();

	auto *videoBuiltin = devInfo->videoBuiltin;
	if (!videoBuiltin) {
		SYSLOG(MODULE_SHORT, "videoBuiltin null");
		for (size_t i = 0; i < devInfo->videoExternal.size(); i++) {
			if (!OSDynamicCast(IOPCIDevice, devInfo->videoExternal[i].video)) { continue; }
			if (WIOKit::readPCIConfigValue(devInfo->videoExternal[i].video, WIOKit::kIOPCIConfigVendorID) ==
				WIOKit::VendorID::ATIAMD) {
				videoBuiltin = devInfo->videoExternal[i].video;
				break;
			}
		}
	}

	PANIC_COND(!videoBuiltin, MODULE_SHORT, "videoBuiltin null");
	auto *iGPU = OSDynamicCast(IOPCIDevice, videoBuiltin);
	PANIC_COND(!iGPU, MODULE_SHORT, "videoBuiltin is not IOPCIDevice");
	PANIC_COND(WIOKit::readPCIConfigValue(iGPU, WIOKit::kIOPCIConfigVendorID) != WIOKit::VendorID::ATIAMD, MODULE_SHORT,
		"videoBuiltin is not AMD");

	callbackLRed->iGPU = iGPU;

	WIOKit::renameDevice(iGPU, "IGPU");
	WIOKit::awaitPublishing(iGPU);

    static uint8_t builtin[] = {0x01};
    iGPU->setProperty("built-in", builtin, sizeof(builtin));
    callbackLRed->deviceId = WIOKit::readPCIConfigValue(iGPU, WIOKit::kIOPCIConfigDeviceID);
    auto *model = getBranding(callbackLRed->deviceId, WIOKit::readPCIConfigValue(iGPU, WIOKit::kIOPCIConfigRevisionID));
    if (model) { iGPU->setProperty("model", model); }

    if (UNLIKELY(iGPU->getProperty("ATY,bin_image"))) {
        DBGLOG(MODULE_SHORT, "VBIOS manually overridden");
    } else {
        if (!callbackLRed->getVBIOSFromVFCT(iGPU)) {
            SYSLOG(MODULE_SHORT, "Failed to get VBIOS from VFCT.");
            PANIC_COND(!callbackLRed->getVBIOSFromVRAM(iGPU), MODULE_SHORT, "Failed to get VBIOS from VRAM");
        }
    }

    callbackLRed->rmmio = iGPU->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress5);
    PANIC_COND(!callbackLRed->rmmio || !callbackLRed->rmmio->getLength(), MODULE_SHORT, "Failed to map RMMIO");
    callbackLRed->rmmioPtr = reinterpret_cast<uint32_t *>(callbackLRed->rmmio->getVirtualAddress());

    callbackLRed->fbOffset = static_cast<uint64_t>(callbackLRed->readReg32(0x296B)) << 24;
    callbackLRed->revision = (callbackLRed->readReg32(0xD2F) & 0xF000000) >> 0x18;
    switch (callbackLRed->deviceId) {
        case 0x15D8:
            PANIC("lred", "Vega iGPU detected! Use WhateverRed instead");
            break;
        case 0x15DD:
            PANIC("lred", "Vega iGPU detected! Use WhateverRed instead");
            break;
        case 0x164C:
            [[fallthrough]];
        case 0x1636:
            PANIC("lred", "Vega iGPU detected! Use WhateverRed instead");
            break;
        case 0x15E7:
            [[fallthrough]];
        case 0x1638:
            PANIC("lred", "Vega iGPU detected! Use WhateverRed instead");
            break;
        case 0x9850:
            callbackLRed->chipType = ChipType::Mullins;
            DBGLOG("lred", "Detected APU as Mullins");
            break;
        case 0x9851:
            callbackLRed->chipType = ChipType::Mullins;
            DBGLOG("lred", "Detected APU as Mullins");
            break;
        case 0x9852:
            callbackLRed->chipType = ChipType::Mullins;
            DBGLOG("lred", "Detected APU as Mullins");
            break;
        case 0x9853:
            callbackLRed->chipType = ChipType::Mullins;
            DBGLOG("lred", "Detected APU as Mullins");
            break;
        case 0x9854:
            callbackLRed->chipType = ChipType::Mullins;
            DBGLOG("lred", "Detected APU as Mullins");
            break;
        case 0x9855:
            callbackLRed->chipType = ChipType::Mullins;
            DBGLOG("lred", "Detected APU as Mullins");
            break;
        case 0x9856:
            callbackLRed->chipType = ChipType::Mullins;
            DBGLOG("lred", "Detected APU as Mullins");
            break;
        case 0x9874:
            callbackLRed->chipType = ChipType::Carrizo;
            DBGLOG("lred", "Detected APU as Carrizo");
            break;
        case 0x98E4:
            callbackLRed->chipType = ChipType::Stoney;
            DBGLOG("lred", "Detected APU as Stoney");
            break;
        default:
            PANIC("lred", "Unknown device ID");
    }

    DeviceInfo::deleter(devInfo);
}
OSMetaClassBase *LRed::wrapSafeMetaCast(const OSMetaClassBase *anObject, const OSMetaClass *toMeta) {
    auto ret = FunctionCast(wrapSafeMetaCast, callbackLRed->orgSafeMetaCast)(anObject, toMeta);
    if (!ret) {
        for (const auto &ent : callbackLRed->metaClassMap) {
            if (ent[0] == toMeta) {
                return FunctionCast(wrapSafeMetaCast, callbackLRed->orgSafeMetaCast)(anObject, ent[1]);
            } else if (ent[1] == toMeta) {
                return FunctionCast(wrapSafeMetaCast, callbackLRed->orgSafeMetaCast)(anObject, ent[0]);
            }
        }
    }
    return ret;
}

void LRed::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextRadeonX4000HWLibs.loadIndex == index) {
        KernelPatcher::SolveRequest solveRequests[] = {
            //{"__ZL15deviceTypeTable", orgDeviceTypeTable},
            //{"__ZN11AMDFirmware14createFirmwareEPhjjPKc", orgCreateFirmware},
            //{"__ZN20AMDFirmwareDirectory11putFirmwareE16_AMD_DEVICE_TYPEP11AMDFirmware", orgPutFirmware},
            {"__ZN31AtiAppleHawaiiPowerTuneServicesC1EP11PP_InstanceP18PowerPlayCallbacks",
                orgVega10PowerTuneConstructor},
            {"__ZL20CAIL_ASIC_CAPS_TABLE", orgCapsTableHWLibs}, {"_CAILAsicCapsInitTable", orgAsicInitCapsTable},
            {"_Carrizo_RLC_UCODE", orgCzRlcUcode}, {"_Carrizo_ME_UCODE", orgCzMeUcode},
            {"_Carrizo_CE_UCODE", orgCzCeUcode}, {"_Carrizo_PFP_UCODE", orgCzPfpUcode},
            {"_Carrizo_MEC1_UCODE", orgCzMecUcode}, {"_Carrizo_MEC2_UCODE", orgCzMec2Ucode},
            //{"_Carrizo_SDMA0_UCODE", orgCzSdma0Ucode}, {"_Carrizo_SDMA1_UCODE", orgCzSdma1Ucode},
            {"_Stoney_RLC_UCODE", orgStnyRlcUcode}, {"_Stoney_ME_UCODE", orgStnyMeUcode},
            {"_Stoney_CE_UCODE", orgStnyCeUcode}, {"_Stoney_PFP_UCODE", orgStnyPfpUcode},
            //{"_Stoney_MEC1_UCODE", orgStnyMecUcode}, {"_Stoney_SDMA0_UCODE", orgStnySdmaUcode},
            //{"_Raven_SendMsgToSmc", orgRavenSendMsgToSmc},
            //{"_Renoir_SendMsgToSmc", orgRenoirSendMsgToSmc},
        };
        PANIC_COND(!patcher.solveMultiple(index, solveRequests, address, size), "lred",
            "Failed to resolve AMDRadeonX5000HWLibs symbols");

        KernelPatcher::RouteRequest requests[] = {
            {"__ZN15AmdCailServicesC2EP11IOPCIDevice", wrapAmdTtlServicesConstructor, orgAmdTtlServicesConstructor},
            //{"__ZN35AMDRadeonX5000_AMDRadeonHWLibsX500025populateFirmwareDirectoryEv", wrapPopulateFirmwareDirectory,
            // orgPopulateFirmwareDirectory},
            {"__ZN25AtiApplePowerTuneServices23createPowerTuneServicesEP11PP_InstanceP18PowerPlayCallbacks",
                wrapCreatePowerTuneServices},
            //{"_SmuRaven_Initialize", wrapSmuRavenInitialize, orgSmuRavenInitialize},
            //{"_SmuRenoir_Initialize", wrapSmuRenoirInitialize, orgSmuRenoirInitialize},
            {"__ZN15AmdCailServices23queryEngineRunningStateEP17CailHwEngineQueueP22CailEngineRunningState",
                wrapQueryEngineRunningState, orgCAILQueryEngineRunningState},
            {"_CailMonitorPerformanceCounter", wrapCailMonitorPerformanceCounter, orgCailMonitorPerformanceCounter},
            {"_Cail_MCILTrace0", wrapCailMCILTrace0, orgCailMCILTrace0},
            {"_Cail_MCILTrace1", wrapCailMCILTrace1, orgCailMCILTrace1},
            {"_MCILDebugPrint", wrapMCILDebugPrint, orgMCILDebugPrint},
            {"_SMUM_Initialize", wrapSMUMInitialize, orgSMUMInitialize},
        };
        PANIC_COND(!patcher.routeMultiple(index, requests, address, size), "lred",
            "Failed to route AMDRadeonX4000HWLibs symbols");

        auto *asicName = callbackLRed->getASICName();
        // TODO: Inject all parts of RLC firmware
        // Disabled firmware injection due to bad code
    } else if (kextRadeonFramebuffer.loadIndex == index) {
        /*        KernelPatcher::SolveRequest solveRequests[] = {
                    {"__ZL20CAIL_ASIC_CAPS_TABLE", orgAsicCapsTable},
                };
                PANIC_COND(!patcher.solveMultiple(index, solveRequests, address, size, true), "lred",
                    "Failed to resolve AMDFramebuffer symbols");

                KernelPatcher::RouteRequest requests[] = {
                    {"__ZNK15AmdAtomVramInfo16populateVramInfoER16AtomFirmwareInfo", wrapPopulateVramInfo},
                    {"__ZN30AMDRadeonX6000_AmdAsicInfoNavi18populateDeviceInfoEv", wrapPopulateDeviceInfo,
                        orgPopulateDeviceInfo},
                    {"__ZNK32AMDRadeonX6000_AmdAsicInfoNavi1027getEnumeratedRevisionNumberEv",
           wrapGetEnumeratedRevision},
                    {"__ZN32AMDRadeonX6000_AmdRegisterAccess11hwReadReg32Ej", wrapHwReadReg32, orgHwReadReg32},
                    {"__ZN24AMDRadeonX6000_AmdLogger15initWithPciInfoEP11IOPCIDevice", wrapInitWithPciInfo,
           orgInitWithPciInfo},
                    {"__ZN34AMDRadeonX6000_AmdRadeonController10doGPUPanicEPKcz", wrapDoGPUPanic},
                };
                PANIC_COND(!patcher.routeMultiple(index, requests, address, size), "lred",
                    "Failed to route AMDRadeonX6000Framebuffer symbols");
        */
    } else if (kextRadeonX4000.loadIndex == index) {
        uint32_t *orgChannelTypes = nullptr;

        KernelPatcher::SolveRequest solveRequests[] = {
            {"__ZN29AMDRadeonX4000_AMDCIPM4EngineC1Ev", orgGFX9PM4EngineConstructor},
            //{"__ZN32AMDRadeonX5000_AMDGFX9SDMAEngineC1Ev", orgGFX9SDMAEngineConstructor},
            //{"__ZZN37AMDRadeonX5000_AMDGraphicsAccelerator19createAccelChannelsEbE12channelTypes", orgChannelTypes},
            //{"__ZN39AMDRadeonX5000_AMDAccelSharedUserClient5startEP9IOService", orgAccelSharedUCStart},
            //{"__ZN39AMDRadeonX5000_AMDAccelSharedUserClient4stopEP9IOService", orgAccelSharedUCStop},
            //{"__ZN35AMDRadeonX5000_AMDAccelVideoContext10gMetaClassE", metaClassMap[0][0]},
            //{"__ZN37AMDRadeonX5000_AMDAccelDisplayMachine10gMetaClassE", metaClassMap[1][0]},
            //{"__ZN34AMDRadeonX5000_AMDAccelDisplayPipe10gMetaClassE", metaClassMap[2][0]},
            //{"__ZN30AMDRadeonX5000_AMDAccelChannel10gMetaClassE", metaClassMap[3][1]},
        };
        PANIC_COND(!patcher.solveMultiple(index, solveRequests, address, size), "lred",
            "Failed to resolve AMDRadeonX4000 symbols");

        /** I have to port this to X4000, eventually. */ /*
        PANIC_COND(MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) != KERN_SUCCESS, "lred",
            "Failed to enable kernel writing");
        orgChannelTypes[5] = 1;     // Fix createAccelChannels
        orgChannelTypes[11] = 0;    // Fix getPagingChannel
        MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
                */
        KernelPatcher::RouteRequest requests[] = {
            {"__ZN28AMDRadeonX4000_AMDCIHardware17allocateHWEnginesEv", wrapAllocateHWEngines},
            {"__ZN28AMDRadeonX4000_AMDCIHardware32setupAndInitializeHWCapabilitiesEv",
                wrapSetupAndInitializeHWCapabilities, orgSetupAndInitializeHWCapabilities},
            //{"__ZN28AMDRadeonX5000_AMDRTHardware12getHWChannelE18_eAMD_CHANNEL_TYPE11SS_PRIORITYj",
            // wrapRTGetHWChannel, orgRTGetHWChannel},
            //{"__ZN30AMDRadeonX5000_AMDGFX9Hardware20initializeFamilyTypeEv", wrapInitializeFamilyType},
            //{"__ZN30AMDRadeonX5000_AMDGFX9Hardware20allocateAMDHWDisplayEv", wrapAllocateAMDHWDisplay},
            //{"__ZN41AMDRadeonX5000_AMDGFX9GraphicsAccelerator15newVideoContextEv", wrapNewVideoContext},
            //{"__ZN31AMDRadeonX5000_IAMDSMLInterface18createSMLInterfaceEj", wrapCreateSMLInterface},
            //{"__ZN26AMDRadeonX5000_AMDHWMemory17adjustVRAMAddressEy", wrapAdjustVRAMAddress, orgAdjustVRAMAddress},
            //{"__ZN37AMDRadeonX5000_AMDGraphicsAccelerator9newSharedEv", wrapNewShared},
            //{"__ZN37AMDRadeonX5000_AMDGraphicsAccelerator19newSharedUserClientEv", wrapNewSharedUserClient},
            //{"__ZN30AMDRadeonX5000_AMDGFX9Hardware25allocateAMDHWAlignManagerEv", wrapAllocateAMDHWAlignManager,
            // orgAllocateAMDHWAlignManager},
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator15configureDeviceEP11IOPCIDevice", wrapConfigureDevice,
                orgConfigureDevice},
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator15createHWHandlerEv", wrapCreateHWHandler, orgCreateHWHandler},
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator17createHWInterfaceEP11IOPCIDevice", wrapCreateHWInterface,
                orgCreateHWInterface},
            {"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator5startEP9IOService", wrapAccelStart, orgAccelStart},
        };
        PANIC_COND(!patcher.routeMultiple(index, requests, address, size), "lred",
            "Failed to route AMDRadeonX4000 symbols");

        /**
         * `AMDRadeonX4000_AMDHardware::startHWEngines`
         * Make for loop stop at 1 instead of 2 since we only have one SDMA engine.
         */
        /*
if (this->chipType == ChipType::Stoney) {
    const uint8_t find[] = {0x49, 0x89, 0xFE, 0x31, 0xDB, 0x48, 0x83, 0xFB, 0x02, 0x74,
        0x50};    // probably broken
    const uint8_t repl[] = {0x49, 0x89, 0xFE, 0x31, 0xDB, 0x48, 0x83, 0xFB, 0x01, 0x74, 0x50};
    static_assert(arrsize(find) == arrsize(repl));
    KernelPatcher::LookupPatch patch = {&kextRadeonX4000, find, repl, arrsize(find), 1};
    patcher.applyLookupPatch(&patch);
    patcher.clearError();
    DBGLOG("lred", "-- Stoney Singular SDMA patch done, Let's hope we didn't break X4000 --");
}
         */ // High chances are that this patch is broken, Will do eventually.

    } else if (kextAMD8000Controller.loadIndex == index) {
        KernelPatcher::RouteRequest requests[] = {
            {"__ZN13ASIC_INFO__CIC2Ev", wrapASICINFO_CI, orgASICINFO_CI},
            {"__ZN24DEVICE_COMPONENT_FACTORY14createAsicInfoEP13ATIController", wrapCreateAsicInfo, orgCreateAsicInfo},
            {"__ZN17AMD8000Controller15powerUpHardwareEv", wrapPowerUpHardware, orgPowerUpHardware},
            {"__ZN17AMD8000Controller35initializeProjectDependentResourcesEv", wrapInitializeProjectDependentResources,
                orgInitializeProjectDependentResources},
        };
        PANIC_COND(!patcher.routeMultipleLong(index, requests, address, size), "lred",
            "Failed to route AMD8000Controller symbols");
        DBGLOG("lred", "Successfully routed AMD8000Controller symbols");
    }
}

void LRed::wrapAmdTtlServicesConstructor(void *that, IOPCIDevice *provider) {
    DBGLOG("lred", "AmdCailServices constructor called!");

    FunctionCast(wrapAmdTtlServicesConstructor, callbackLRed->orgAmdTtlServicesConstructor)(that, provider);
}

uint32_t LRed::wrapGcGetHwVersion() { return 0x090201; }

void *LRed::wrapCreatePowerTuneServices(void *that, void *param2) {
    auto *ret = IOMallocZero(0x18);
    callbackLRed->orgVega10PowerTuneConstructor(ret, that, param2);
    return ret;
}

uint16_t LRed::wrapGetEnumeratedRevision() { return callbackLRed->enumeratedRevision; }

IOReturn LRed::wrapPopulateDeviceInfo(void *that) {
    auto ret = FunctionCast(wrapPopulateDeviceInfo, callbackLRed->orgPopulateDeviceInfo)(that);
    auto deviceId = WIOKit::readPCIConfigValue(getMember<IOPCIDevice *>(that, 0x18), WIOKit::kIOPCIConfigDeviceID);
    getMember<uint32_t>(that, 0x60) = AMDGPU_FAMILY_CZ;
    auto revision = getMember<uint32_t>(that, 0x68);
    auto emulatedRevision = getMember<uint32_t>(that, 0x6c);

    DBGLOG("lred", "Locating Init Caps entry");
    PANIC_COND(MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) != KERN_SUCCESS, "lred",
        "Failed to enable kernel writing");

    CailInitAsicCapEntry *initCaps = nullptr;
    for (size_t i = 0; i < 789; i++) {
        auto *temp = callbackLRed->orgAsicInitCapsTable + i;
        if (temp->familyId == AMDGPU_FAMILY_CZ && temp->deviceId == deviceId && temp->revision == revision) {
            initCaps = temp;
            break;
        }
    }

    if (!initCaps) {
        DBGLOG("lred", "Warning: Using fallback Init Caps search");
        for (size_t i = 0; i < 789; i++) {
            auto *temp = callbackLRed->orgAsicInitCapsTable + i;
            if (temp->familyId == AMDGPU_FAMILY_CZ && temp->deviceId == deviceId) {
                initCaps = temp;
                initCaps->emulatedRev = emulatedRevision;
                initCaps->revision = revision;
                initCaps->pciRev = 0xFFFFFFFF;
                break;
            }
        }
        PANIC_COND(!initCaps, "lred", "Failed to find Init Caps entry");
    }

    callbackLRed->orgCapsTableHWLibs->familyId = callbackLRed->orgAsicCapsTable->familyId = AMDGPU_FAMILY_CZ;
    callbackLRed->orgCapsTableHWLibs->deviceId = callbackLRed->orgAsicCapsTable->deviceId = deviceId;
    callbackLRed->orgCapsTableHWLibs->revision = callbackLRed->orgAsicCapsTable->revision = revision;
    callbackLRed->orgCapsTableHWLibs->emulatedRev = callbackLRed->orgAsicCapsTable->emulatedRev = emulatedRevision;
    callbackLRed->orgCapsTableHWLibs->pciRev = callbackLRed->orgAsicCapsTable->pciRev = 0xFFFFFFFF;
    callbackLRed->orgCapsTableHWLibs->caps = callbackLRed->orgAsicCapsTable->caps = initCaps->caps;
    MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);

    return ret;
}

uint32_t LRed::hwLibsNoop() { return 0; }           // Always return success
uint32_t LRed::hwLibsUnsupported() { return 4; }    // Always return unsupported

IOReturn LRed::wrapPopulateVramInfo([[maybe_unused]] void *that, void *fwInfo) {
    auto *vbios = static_cast<const uint8_t *>(callbackLRed->vbiosData->getBytesNoCopy());
    auto base = *reinterpret_cast<const uint16_t *>(vbios + ATOM_ROM_TABLE_PTR);
    auto dataTable = *reinterpret_cast<const uint16_t *>(vbios + base + ATOM_ROM_DATA_PTR);
    auto *mdt = reinterpret_cast<const uint16_t *>(vbios + dataTable + 4);
    uint32_t channelCount = 1;
    if (mdt[0x1E]) {
        DBGLOG("lred", "Fetching VRAM info from iGPU System Info");
        uint32_t offset = 0x1E * 2 + 4;
        auto index = *reinterpret_cast<const uint16_t *>(vbios + dataTable + offset);
        auto *table = reinterpret_cast<const IgpSystemInfo *>(vbios + index);
        switch (table->header.formatRev) {
            case 1:
                switch (table->header.contentRev) {
                    case 11:
                        [[fallthrough]];
                    case 12:
                        if (table->infoV11.umaChannelCount) channelCount = table->infoV11.umaChannelCount;
                        break;
                    default:
                        DBGLOG("lred", "Unsupported contentRev %d", table->header.contentRev);
                }
            case 2:
                switch (table->header.contentRev) {
                    case 1:
                        [[fallthrough]];
                    case 2:
                        if (table->infoV2.umaChannelCount) channelCount = table->infoV2.umaChannelCount;
                        break;
                    default:
                        DBGLOG("lred", "Unsupported contentRev %d", table->header.contentRev);
                }
            default:
                DBGLOG("lred", "Unsupported formatRev %d", table->header.formatRev);
        }
    } else {
        DBGLOG("lred", "No iGPU System Info in Master Data Table");
    }
    getMember<uint32_t>(fwInfo, 0x1C) = 4;                    // VRAM Type (DDR4)
    getMember<uint32_t>(fwInfo, 0x20) = channelCount * 64;    // VRAM Width (64-bit channels)
    return kIOReturnSuccess;
}

/**
 * We don't want the `AMDRadeonX6000` personality defined in the `Info.plist` to do anything.
 * We only use it to force-load `AMDRadeonX6000` and snatch the VCN/DCN symbols.
 */
bool LRed::wrapAccelStartX6000() { return false; }

bool LRed::wrapAllocateHWEngines(void *that) {
    auto *pm4 = IOMallocZero(0x1E8);
    callbackLRed->orgGFX9PM4EngineConstructor(pm4);
    getMember<void *>(that, 0x3B8) = pm4;

    auto *sdma0 = IOMallocZero(0x128);
    callbackLRed->orgGFX9SDMAEngineConstructor(sdma0);
    getMember<void *>(that, 0x3C0) = sdma0;

    auto *vcn2 = IOMallocZero(0x198);
    callbackLRed->orgVCN2EngineConstructorX6000(vcn2);
    getMember<void *>(that, 0x3F8) = vcn2;
    return true;
}

void LRed::wrapSetupAndInitializeHWCapabilities(void *that) {
    FunctionCast(wrapSetupAndInitializeHWCapabilities, callbackLRed->orgSetupAndInitializeHWCapabilities)(that);
}

/**
 * Hack: Add custom param 4 and 5 (pointer to firmware and size)
 * aka RCX and R8 registers
 * Complementary to `_psp_asd_load` patch-set.
 */

void *LRed::wrapRTGetHWChannel(void *that, uint32_t param1, uint32_t param2, uint32_t param3) {
    if (param1 == 2 && param2 == 0 && param3 == 0) { param2 = 2; }    // Redirect SDMA1 retrieval to SDMA0
    return FunctionCast(wrapRTGetHWChannel, callbackLRed->orgRTGetHWChannel)(that, param1, param2, param3);
}

uint32_t LRed::wrapHwReadReg32(void *that, uint32_t reg) {
    if (!callbackLRed->fbOffset) {
        callbackLRed->fbOffset =
            static_cast<uint64_t>(FunctionCast(wrapHwReadReg32, callbackLRed->orgHwReadReg32)(that, 0x296B)) << 24;
    }
    return FunctionCast(wrapHwReadReg32, callbackLRed->orgHwReadReg32)(that, reg == 0xD31 ? 0xD2F : reg);
}

uint32_t LRed::wrapSmuRavenInitialize(void *smum, uint32_t param2) {
    auto ret = FunctionCast(wrapSmuRavenInitialize, callbackLRed->orgSmuRavenInitialize)(smum, param2);
    callbackLRed->orgRavenSendMsgToSmc(smum, PPSMC_MSG_PowerUpSdma);
    callbackLRed->orgRavenSendMsgToSmc(smum, PPSMC_MSG_PowerGateMmHub);
    return ret;
}

uint32_t LRed::wrapSmuRenoirInitialize(void *smum, uint32_t param2) {
    auto ret = FunctionCast(wrapSmuRenoirInitialize, callbackLRed->orgSmuRenoirInitialize)(smum, param2);
    callbackLRed->orgRenoirSendMsgToSmc(smum, PPSMC_MSG_PowerUpSdma);
    callbackLRed->orgRenoirSendMsgToSmc(smum, PPSMC_MSG_PowerGateMmHub);
    return ret;
}

void LRed::wrapInitializeFamilyType(void *that) { getMember<uint32_t>(that, 0x308) = AMDGPU_FAMILY_CZ; }

void *LRed::wrapAllocateAMDHWDisplay(void *that) {
    return FunctionCast(wrapAllocateAMDHWDisplay, callbackLRed->orgAllocateAMDHWDisplayX6000)(that);
}

bool LRed::wrapInitWithPciInfo(void *that, void *param1) {
    auto ret = FunctionCast(wrapInitWithPciInfo, callbackLRed->orgInitWithPciInfo)(that, param1);
    // Hack AMDRadeonX6000_AmdLogger to log everything
    getMember<uint64_t>(that, 0x28) = ~0ULL;
    getMember<uint32_t>(that, 0x30) = 0xFF;
    return ret;
}

void *LRed::wrapNewVideoContext(void *that) {
    return FunctionCast(wrapNewVideoContext, callbackLRed->orgNewVideoContextX6000)(that);
}

void *LRed::wrapCreateSMLInterface(uint32_t configBit) {
    return FunctionCast(wrapCreateSMLInterface, callbackLRed->orgCreateSMLInterfaceX6000)(configBit);
}

uint64_t LRed::wrapAdjustVRAMAddress(void *that, uint64_t addr) {
    auto ret = FunctionCast(wrapAdjustVRAMAddress, callbackLRed->orgAdjustVRAMAddress)(that, addr);
    return ret != addr ? (ret + callbackLRed->fbOffset) : ret;
}

void *LRed::wrapNewShared() { return FunctionCast(wrapNewShared, callbackLRed->orgNewSharedX6000)(); }

void *LRed::wrapNewSharedUserClient() {
    return FunctionCast(wrapNewSharedUserClient, callbackLRed->orgNewSharedUserClientX6000)();
}

bool LRed::wrapAccelSharedUCStartX6000(void *that, void *provider) {
    return FunctionCast(wrapAccelSharedUCStartX6000, callbackLRed->orgAccelSharedUCStart)(that, provider);
}

bool LRed::wrapAccelSharedUCStopX6000(void *that, void *provider) {
    return FunctionCast(wrapAccelSharedUCStopX6000, callbackLRed->orgAccelSharedUCStop)(that, provider);
}
/*
void LRed::wrapInitDCNRegistersOffsets(void *that) {
    FunctionCast(wrapInitDCNRegistersOffsets, callbackLRed->orgInitDCNRegistersOffsets)(that);
    if (callbackLRed->chipType < ChipType::Renoir) {
        DBGLOG("lred", "initDCNRegistersOffsets !! PATCHING REGISTERS FOR DCN 1.0 !!");
        auto base = getMember<uint32_t>(that, 0x4830);
        getMember<uint32_t>(that, 0x4840) = base + mmHUBPREQ0_DCSURF_PRIMARY_SURFACE_ADDRESS;
        getMember<uint32_t>(that, 0x4878) = base + mmHUBPREQ1_DCSURF_PRIMARY_SURFACE_ADDRESS;
        getMember<uint32_t>(that, 0x48B0) = base + mmHUBPREQ2_DCSURF_PRIMARY_SURFACE_ADDRESS;
        getMember<uint32_t>(that, 0x48E8) = base + mmHUBPREQ3_DCSURF_PRIMARY_SURFACE_ADDRESS;
        getMember<uint32_t>(that, 0x4844) = base + mmHUBPREQ0_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH;
        getMember<uint32_t>(that, 0x487C) = base + mmHUBPREQ1_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH;
        getMember<uint32_t>(that, 0x48B4) = base + mmHUBPREQ2_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH;
        getMember<uint32_t>(that, 0x48EC) = base + mmHUBPREQ3_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH;
        getMember<uint32_t>(that, 0x4848) = base + mmHUBP0_DCSURF_SURFACE_CONFIG;
        getMember<uint32_t>(that, 0x4880) = base + mmHUBP1_DCSURF_SURFACE_CONFIG;
        getMember<uint32_t>(that, 0x48B8) = base + mmHUBP2_DCSURF_SURFACE_CONFIG;
        getMember<uint32_t>(that, 0x48F0) = base + mmHUBP3_DCSURF_SURFACE_CONFIG;
        getMember<uint32_t>(that, 0x484C) = base + mmHUBPREQ0_DCSURF_SURFACE_PITCH;
        getMember<uint32_t>(that, 0x4884) = base + mmHUBPREQ1_DCSURF_SURFACE_PITCH;
        getMember<uint32_t>(that, 0x48BC) = base + mmHUBPREQ2_DCSURF_SURFACE_PITCH;
        getMember<uint32_t>(that, 0x48F4) = base + mmHUBPREQ3_DCSURF_SURFACE_PITCH;
        getMember<uint32_t>(that, 0x4850) = base + mmHUBP0_DCSURF_ADDR_CONFIG;
        getMember<uint32_t>(that, 0x4888) = base + mmHUBP1_DCSURF_ADDR_CONFIG;
        getMember<uint32_t>(that, 0x48C0) = base + mmHUBP2_DCSURF_ADDR_CONFIG;
        getMember<uint32_t>(that, 0x48F8) = base + mmHUBP3_DCSURF_ADDR_CONFIG;
        getMember<uint32_t>(that, 0x4854) = base + mmHUBP0_DCSURF_TILING_CONFIG;
        getMember<uint32_t>(that, 0x488C) = base + mmHUBP1_DCSURF_TILING_CONFIG;
        getMember<uint32_t>(that, 0x48C4) = base + mmHUBP2_DCSURF_TILING_CONFIG;
        getMember<uint32_t>(that, 0x48FC) = base + mmHUBP3_DCSURF_TILING_CONFIG;
        getMember<uint32_t>(that, 0x4858) = base + mmHUBP0_DCSURF_PRI_VIEWPORT_START;
        getMember<uint32_t>(that, 0x4890) = base + mmHUBP1_DCSURF_PRI_VIEWPORT_START;
        getMember<uint32_t>(that, 0x48C8) = base + mmHUBP2_DCSURF_PRI_VIEWPORT_START;
        getMember<uint32_t>(that, 0x4900) = base + mmHUBP3_DCSURF_PRI_VIEWPORT_START;
        getMember<uint32_t>(that, 0x485C) = base + mmHUBP0_DCSURF_PRI_VIEWPORT_DIMENSION;
        getMember<uint32_t>(that, 0x4894) = base + mmHUBP1_DCSURF_PRI_VIEWPORT_DIMENSION;
        getMember<uint32_t>(that, 0x48CC) = base + mmHUBP2_DCSURF_PRI_VIEWPORT_DIMENSION;
        getMember<uint32_t>(that, 0x4904) = base + mmHUBP3_DCSURF_PRI_VIEWPORT_DIMENSION;
        getMember<uint32_t>(that, 0x4860) = base + mmOTG0_OTG_CONTROL;
        getMember<uint32_t>(that, 0x4898) = base + mmOTG1_OTG_CONTROL;
        getMember<uint32_t>(that, 0x48D0) = base + mmOTG2_OTG_CONTROL;
        getMember<uint32_t>(that, 0x4908) = base + mmOTG3_OTG_CONTROL;
        getMember<uint32_t>(that, 0x4940) = base + mmOTG4_OTG_CONTROL;
        getMember<uint32_t>(that, 0x4978) = base + mmOTG5_OTG_CONTROL;
        getMember<uint32_t>(that, 0x4864) = base + mmOTG0_OTG_INTERLACE_CONTROL;
        getMember<uint32_t>(that, 0x489C) = base + mmOTG1_OTG_INTERLACE_CONTROL;
        getMember<uint32_t>(that, 0x48D4) = base + mmOTG2_OTG_INTERLACE_CONTROL;
        getMember<uint32_t>(that, 0x490C) = base + mmOTG3_OTG_INTERLACE_CONTROL;
        getMember<uint32_t>(that, 0x4944) = base + mmOTG4_OTG_INTERLACE_CONTROL;
        getMember<uint32_t>(that, 0x497C) = base + mmOTG5_OTG_INTERLACE_CONTROL;
        getMember<uint32_t>(that, 0x4868) = base + mmHUBPREQ0_DCSURF_FLIP_CONTROL;
        getMember<uint32_t>(that, 0x48A0) = base + mmHUBPREQ1_DCSURF_FLIP_CONTROL;
        getMember<uint32_t>(that, 0x48D8) = base + mmHUBPREQ2_DCSURF_FLIP_CONTROL;
        getMember<uint32_t>(that, 0x4910) = base + mmHUBPREQ3_DCSURF_FLIP_CONTROL;
        getMember<uint32_t>(that, 0x486C) = base + mmHUBPRET0_HUBPRET_CONTROL;
        getMember<uint32_t>(that, 0x48A4) = base + mmHUBPRET1_HUBPRET_CONTROL;
        getMember<uint32_t>(that, 0x48DC) = base + mmHUBPRET2_HUBPRET_CONTROL;
        getMember<uint32_t>(that, 0x4914) = base + mmHUBPRET3_HUBPRET_CONTROL;
        getMember<uint32_t>(that, 0x4870) = base + mmHUBPREQ0_DCSURF_SURFACE_EARLIEST_INUSE;
        getMember<uint32_t>(that, 0x48A8) = base + mmHUBPREQ1_DCSURF_SURFACE_EARLIEST_INUSE;
        getMember<uint32_t>(that, 0x48E0) = base + mmHUBPREQ2_DCSURF_SURFACE_EARLIEST_INUSE;
        getMember<uint32_t>(that, 0x4918) = base + mmHUBPREQ3_DCSURF_SURFACE_EARLIEST_INUSE;
        getMember<uint32_t>(that, 0x4874) = base + mmHUBPREQ0_DCSURF_SURFACE_EARLIEST_INUSE_HIGH;
        getMember<uint32_t>(that, 0x48AC) = base + mmHUBPREQ1_DCSURF_SURFACE_EARLIEST_INUSE_HIGH;
        getMember<uint32_t>(that, 0x48E4) = base + mmHUBPREQ2_DCSURF_SURFACE_EARLIEST_INUSE_HIGH;
        getMember<uint32_t>(that, 0x491C) = base + mmHUBPREQ3_DCSURF_SURFACE_EARLIEST_INUSE_HIGH;
    }
}
*/
void *LRed::wrapAllocateAMDHWAlignManager() {
    auto ret = FunctionCast(wrapAllocateAMDHWAlignManager, callbackLRed->orgAllocateAMDHWAlignManager)();
    callbackLRed->hwAlignMgr = ret;

    callbackLRed->hwAlignMgrVtX5000 = getMember<uint8_t *>(ret, 0);
    callbackLRed->hwAlignMgrVtX6000 = static_cast<uint8_t *>(IOMallocZero(0x238));

    memcpy(callbackLRed->hwAlignMgrVtX6000, callbackLRed->hwAlignMgrVtX5000, 0x128);
    *reinterpret_cast<mach_vm_address_t *>(callbackLRed->hwAlignMgrVtX6000 + 0x128) =
        callbackLRed->orgGetPreferredSwizzleMode2;
    memcpy(callbackLRed->hwAlignMgrVtX6000 + 0x130, callbackLRed->hwAlignMgrVtX5000 + 0x128, 0x230 - 0x128);
    return ret;
}

#define HWALIGNMGR_ADJUST getMember<void *>(callbackLRed->hwAlignMgr, 0) = callbackLRed->hwAlignMgrVtX6000;
#define HWALIGNMGR_REVERT getMember<void *>(callbackLRed->hwAlignMgr, 0) = callbackLRed->hwAlignMgrVtX5000;

uint64_t LRed::wrapAccelSharedSurfaceCopy(void *that, void *param1, uint64_t param2, void *param3) {
    HWALIGNMGR_ADJUST
    auto ret =
        FunctionCast(wrapAccelSharedSurfaceCopy, callbackLRed->orgAccelSharedSurfaceCopy)(that, param1, param2, param3);
    HWALIGNMGR_REVERT
    return ret;
}

uint64_t LRed::wrapAllocateScanoutFB(void *that, uint32_t param1, void *param2, void *param3, void *param4) {
    HWALIGNMGR_ADJUST
    auto ret =
        FunctionCast(wrapAllocateScanoutFB, callbackLRed->orgAllocateScanoutFB)(that, param1, param2, param3, param4);
    HWALIGNMGR_REVERT
    return ret;
}

uint64_t LRed::wrapFillUBMSurface(void *that, uint32_t param1, void *param2, void *param3) {
    HWALIGNMGR_ADJUST
    auto ret = FunctionCast(wrapFillUBMSurface, callbackLRed->orgFillUBMSurface)(that, param1, param2, param3);
    HWALIGNMGR_REVERT
    return ret;
}

bool LRed::wrapConfigureDisplay(void *that, uint32_t param1, uint32_t param2, void *param3, void *param4) {
    HWALIGNMGR_ADJUST
    auto ret =
        FunctionCast(wrapConfigureDisplay, callbackLRed->orgConfigureDisplay)(that, param1, param2, param3, param4);
    HWALIGNMGR_REVERT
    return ret;
}

uint64_t LRed::wrapGetDisplayInfo(void *that, uint32_t param1, bool param2, bool param3, void *param4, void *param5) {
    HWALIGNMGR_ADJUST
    auto ret =
        FunctionCast(wrapGetDisplayInfo, callbackLRed->orgGetDisplayInfo)(that, param1, param2, param3, param4, param5);
    HWALIGNMGR_REVERT
    return ret;
}

void LRed::wrapDoGPUPanic() {
    DBGLOG("lred", "doGPUPanic << ()");
    while (true) { IOSleep(3600000); }
}

bool LRed::wrapAccelStart(void *that, IOService *provider) {
    DBGLOG("rad", "accelStart: this = %p provider = %p", that, provider);
    callbackLRed->callbackAccelerator = that;
    auto ret = FunctionCast(wrapAccelStart, callbackLRed->orgAccelStart)(that, provider);
    DBGLOG("rad", "accelStart returned %d", ret);
    return ret;
}

uint32_t LRed::wrapConfigureDevice(void *param1) {
    DBGLOG("lred", "configureDevice: param1 = %p", param1);
    auto ret = FunctionCast(wrapConfigureDevice, callbackLRed->orgConfigureDevice)(param1);
    DBGLOG("lred", "configureDevice returned 0x%X", ret);
    return ret;
}

void LRed::wrapASICINFO_CI(void) {
    DBGLOG("lred", "ASIC_INFO__CI: Called.");
    FunctionCast(wrapASICINFO_CI, callbackLRed->orgASICINFO_CI);
    DBGLOG("lred", "ASIC_INFO__CI finished.");
}

void *LRed::wrapCreateAsicInfo(void *param1) {
    DBGLOG("lred", "createAsicInfo: param1 = %p", param1);
    auto ret = FunctionCast(wrapCreateAsicInfo, callbackLRed->orgCreateAsicInfo)(param1);
    DBGLOG("lred", "createAsicInfo returned %p", ret);
    return ret;
}

uint32_t LRed::wrapPowerUpHardware(void) {
    DBGLOG("lred", "AMD8000Controller::powerUpHardware: Called.");
    auto ret = FunctionCast(wrapPowerUpHardware, callbackLRed->orgPowerUpHardware)();
    DBGLOG("lred", "AMD8000Controller::powerUpHardware: We can't tell if it worked or not, theres no parameters, lets "
                   "hope it did");
    return ret;
}

uint32_t LRed::wrapInitializeProjectDependentResources(void) {
    DBGLOG("lred", "initializeProjectDependentResources: Called.");
    auto ret =
        FunctionCast(wrapInitializeProjectDependentResources, callbackLRed->orgInitializeProjectDependentResources)();
    DBGLOG("lred", "initializeProjectDependentResources returned 0x%X");
    return ret;
}

void *LRed::wrapCreateHWHandler(void) {
    DBGLOG("lred", "createHWHandler: Called.");
    auto ret = FunctionCast(wrapCreateHWHandler, callbackLRed->orgCreateHWHandler)();
    DBGLOG("lred", "createHWHandler returned %p", ret);
    return ret;
}

void *LRed::wrapCreateHWInterface(void *param1) {
    DBGLOG("lred", "createHWInterface: param1 = %p", param1);
    auto ret = FunctionCast(wrapCreateHWInterface, callbackLRed->orgCreateHWInterface)(param1);
    DBGLOG("lred", "createHWInterface returned %p", ret);
    return ret;
}
