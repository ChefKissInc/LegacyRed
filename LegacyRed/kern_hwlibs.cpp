//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_hwlibs.hpp"
#include "kern_lred.hpp"
#include "kern_patches.hpp"
#include <Headers/kern_api.hpp>

static const char *pathRadeonX4050HWLibs = "/System/Library/Extensions/AMDRadeonX4000HWServices.kext/Contents/PlugIns/"
                                           "AMDRadeonX4050HWLibs.kext/Contents/MacOS/AMDRadeonX4050HWLibs";

static const char *pathRadeonX4070HWLibs = "/System/Library/Extensions/AMDRadeonX4000HWServices.kext/Contents/PlugIns/"
                                           "AMDRadeonX4070HWLibs.kext/Contents/MacOS/AMDRadeonX4070HWLibs";

static KernelPatcher::KextInfo kextRadeonX4050HWLibs {"com.apple.kext.AMDRadeonX4050HWLibs", &pathRadeonX4050HWLibs, 1,
    {}, {}, KernelPatcher::KextInfo::Unloaded};

static KernelPatcher::KextInfo kextRadeonX4070HWLibs {"com.apple.kext.AMDRadeonX4070HWLibs", &pathRadeonX4070HWLibs, 1,
    {}, {}, KernelPatcher::KextInfo::Unloaded};

HWLibs *HWLibs::callback = nullptr;

void HWLibs::init() {
    callback = this;
    lilu.onKextLoadForce(&kextRadeonX4050HWLibs);
}

bool HWLibs::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextRadeonX4050HWLibs.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();

        CailAsicCapEntry *orgAsicCapsTable = nullptr;
        CailInitAsicCapEntry *orgAsicInitCapsTable = nullptr;
        const void *goldenSettings[static_cast<uint32_t>(ChipType::Unknown)] = {nullptr};
        const uint32_t *ddiCaps[static_cast<uint32_t>(ChipType::Unknown)] = {nullptr};

        if (LRed::callback->chipVariant == ChipVariant::KLE) {
            SolveRequestPlus solveRequests[] = {
                {"_CAIL_DDI_CAPS_KALINDI_A0_E", goldenSettings[static_cast<uint32_t>(ChipType::Kabini)]},
                {"_KALINDI_GoldenSettings_A0_4882_E", goldenSettings[static_cast<uint32_t>(ChipType::Kabini)]},
                {"_CAIL_DDI_CAPS_KALINDI_A1_E", ddiCaps[static_cast<uint32_t>(ChipType::Mullins)]},
                {"_KALINDI_GoldenSettings_A0_4882_E", goldenSettings[static_cast<uint32_t>(ChipType::Mullins)]},
            };
            PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
                "Failed to resolve symbols");
            DBGLOG("hwlibs", "Using 'E' Kalindi Golden Settings");
        } else if (LRed::callback->chipVariant == ChipVariant::KVE) {
            SolveRequestPlus solveRequests[] = {
                {"_CAIL_DDI_CAPS_SPECTRE_A0_E", ddiCaps[static_cast<uint32_t>(ChipType::Kaveri)]},
                {"_SPECTRE_GoldenSettings_A0_8812_E", goldenSettings[static_cast<uint32_t>(ChipType::Kaveri)]},
            };
            PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
                "Failed to resolve symbols");
            DBGLOG("hwlibs", "Using 'E' Kaveri Golden Settings and DDI Caps");
        } else {
            SolveRequestPlus solveRequests[] = {
                {"_CAIL_DDI_CAPS_KALINDI_A0", ddiCaps[static_cast<uint32_t>(ChipType::Kabini)]},
                {"_KALINDI_GoldenSettings_A0_4882", goldenSettings[static_cast<uint32_t>(ChipType::Kabini)]},
                {"_CAIL_DDI_CAPS_KALINDI_A1", ddiCaps[static_cast<uint32_t>(ChipType::Mullins)]},
                {"_GODAVARI_GoldenSettings_A0_2411", goldenSettings[static_cast<uint32_t>(ChipType::Mullins)]},
                {"_CAIL_DDI_CAPS_SPECTRE_A0", ddiCaps[static_cast<uint32_t>(ChipType::Kaveri)]},
                {"_SPECTRE_GoldenSettings_A0_8812", goldenSettings[static_cast<uint32_t>(ChipType::Kaveri)]},
                /** Spectre appears to be another name for Kaveri, so that's the logic we'll use for it */
            };
            PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
                "Failed to resolve symbols");
            DBGLOG("hwlibs", "Using A0 Kalindi and Kaveri Golden Settings and DDI Caps");
        }

        SolveRequestPlus solveRequests[] = {
            {"__ZN31AtiAppleHawaiiPowerTuneServicesC1EP11PP_InstanceP18PowerPlayCallbacks",
                this->orgHawaiiPowerTuneConstructor},
            {"__ZL20CAIL_ASIC_CAPS_TABLE", orgAsicCapsTable}, {"_CAILAsicCapsInitTable", orgAsicInitCapsTable},
            //{"_Raven_SendMsgToSmc", this->orgRavenSendMsgToSmc},
            //{"_CAIL_DDI_CAPS_RAVEN_A0", ddiCaps[static_cast<uint32_t>(ChipType::Raven)]},
            //{"_RAVEN1_GoldenSettings_A0", goldenSettings[static_cast<uint32_t>(ChipType::Raven)]},
        };
        PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
            "Failed to resolve symbols");

        KernelPatcher::RouteRequest requests[] = {
            {"__ZN15AmdCailServicesC2EP11IOPCIDevice", wrapAmdCailServicesConstructor, orgAmdCailServicesConstructor},
            {"__ZN25AtiApplePowerTuneServices23createPowerTuneServicesEP11PP_InstanceP18PowerPlayCallbacks",
                wrapCreatePowerTuneServices},
            {"__ZN15AmdCailServices23queryEngineRunningStateEP17CailHwEngineQueueP22CailEngineRunningState",
                wrapCAILQueryEngineRunningState, orgCAILQueryEngineRunningState},
            {"_CailMonitorPerformanceCounter", wrapCailMonitorPerformanceCounter, orgCailMonitorPerformanceCounter},
            {"_MCILDebugPrint", wrapMCILDebugPrint, orgMCILDebugPrint},
            {"_SMUM_Initialize", wrapSMUMInitialize, orgSMUMInitialize},
            //{"_SmuRaven_Initialize", wrapSmuRavenInitialize, this->orgSmuRavenInitialize},
        };
        PANIC_COND(!patcher.routeMultiple(index, requests, address, size), "hwlibs", "Failed to route symbols");

        PANIC_COND(MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) != KERN_SUCCESS, "hwlibs",
            "Failed to enable kernel writing");
        orgAsicInitCapsTable->familyId = orgAsicCapsTable->familyId = AMDGPU_FAMILY_KV;
        orgAsicInitCapsTable->deviceId = orgAsicCapsTable->deviceId = LRed::callback->deviceId;
        orgAsicInitCapsTable->revision = orgAsicCapsTable->revision = LRed::callback->revision;
        orgAsicInitCapsTable->emulatedRev = orgAsicCapsTable->emulatedRev =
            static_cast<uint32_t>(LRed::callback->enumeratedRevision) + LRed::callback->revision;
        orgAsicInitCapsTable->pciRev = orgAsicCapsTable->pciRev = 0xFFFFFFFF;
        orgAsicInitCapsTable->caps = orgAsicCapsTable->caps = ddiCaps[static_cast<uint32_t>(LRed::callback->chipType)];
        orgAsicInitCapsTable->goldenCaps = goldenSettings[static_cast<uint32_t>(LRed::callback->chipType)];
        MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
        DBGLOG("hwlibs", "Applied DDI Caps patches");

        return true;
    } else if (kextRadeonX4070HWLibs.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();

        CailAsicCapEntry *orgAsicCapsTable = nullptr;
        CailInitAsicCapEntry *orgAsicInitCapsTable = nullptr;
        const void *goldenSettings[static_cast<uint32_t>(ChipType::Unknown)] = {nullptr};
        const uint32_t *ddiCaps[static_cast<uint32_t>(ChipType::Unknown)] = {nullptr};

        /** The Bristol Ridge else if is a complete shot in the dark, it is not guaranteed it will work */
        if (LRed::callback->chipVariant == ChipVariant::s3CU) {
            SolveRequestPlus solveRequests[] = {
                {"_STONEY_GoldenSettings_A0_3CU", goldenSettings[static_cast<uint32_t>(ChipType::Stoney)]},
            };
            PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
                "Failed to resolve symbols");
            DBGLOG("hwlibs", "Using 3CU Stoney Golden Settings");
        } else if (LRed::callback->chipVariant == ChipVariant::s2CU) {
            SolveRequestPlus solveRequests[] = {
                {"_STONEY_GoldenSettings_A0_2CU", goldenSettings[static_cast<uint32_t>(ChipType::Stoney)]},
            };
            PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
                "Failed to resolve symbols");
            DBGLOG("hwlibs", "Using 2CU Stoney Golden Settings");
        } else if (LRed::callback->chipVariant == ChipVariant::Bristol) {
            SolveRequestPlus solveRequests[] = {
                {"_CARRIZO_GoldenSettings_A1", goldenSettings[static_cast<uint32_t>(ChipType::Carrizo)]},
                {"_CAIL_DDI_CAPS_CARRIZO_A1", ddiCaps[static_cast<uint32_t>(ChipType::Carrizo)]},
            };
            PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
                "Failed to resolve symbols");
            DBGLOG("hwlibs", "Using A1 Carrizo Golden Settings and DDI Caps");
        } else {
            SolveRequestPlus solveRequests[] = {
                {"_CARRIZO_GoldenSettings_A0", goldenSettings[static_cast<uint32_t>(ChipType::Carrizo)]},
                {"_CAIL_DDI_CAPS_CARRIZO_A0", ddiCaps[static_cast<uint32_t>(ChipType::Carrizo)]},
            };
            PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
                "Failed to resolve symbols");
            DBGLOG("hwlibs", "Using A0 Carrizo Golden Settings and DDI Caps");
        }

        SolveRequestPlus solveRequests[] = {
            {"__ZN30AtiAppleTongaPowerTuneServicesC1EP11PP_InstanceP18PowerPlayCallbacks",
                this->orgTongaPowerTuneConstructor},
            {"__ZL20CAIL_ASIC_CAPS_TABLE", orgAsicCapsTable}, {"_CAILAsicCapsInitTable", orgAsicInitCapsTable},
            //{"_Raven_SendMsgToSmc", this->orgRavenSendMsgToSmc},
        };
        PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
            "Failed to resolve symbols");

        KernelPatcher::RouteRequest requests[] = {
            {"__ZN15AmdCailServicesC2EP11IOPCIDevice", wrapAmdCailServicesConstructor, orgAmdCailServicesConstructor},
            {"__ZN25AtiApplePowerTuneServices23createPowerTuneServicesEP11PP_InstanceP18PowerPlayCallbacks",
                wrapCreatePowerTuneServices},
            //{"_SmuRaven_Initialize", wrapSmuRavenInitialize, this->orgSmuRavenInitialize},
        };
        PANIC_COND(!patcher.routeMultiple(index, requests, address, size), "hwlibs", "Failed to route symbols");

        PANIC_COND(MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) != KERN_SUCCESS, "hwlibs",
            "Failed to enable kernel writing");
        orgAsicInitCapsTable->familyId = orgAsicCapsTable->familyId = AMDGPU_FAMILY_CZ;
        orgAsicInitCapsTable->deviceId = orgAsicCapsTable->deviceId = LRed::callback->deviceId;
        orgAsicInitCapsTable->revision = orgAsicCapsTable->revision = LRed::callback->revision;
        orgAsicInitCapsTable->emulatedRev = orgAsicCapsTable->emulatedRev =
            static_cast<uint32_t>(LRed::callback->enumeratedRevision) + LRed::callback->revision;
        orgAsicInitCapsTable->pciRev = orgAsicCapsTable->pciRev = 0xFFFFFFFF;
        orgAsicInitCapsTable->caps = orgAsicCapsTable->caps = ddiCaps[static_cast<uint32_t>(LRed::callback->chipType)];
        orgAsicInitCapsTable->goldenCaps = goldenSettings[static_cast<uint32_t>(LRed::callback->chipType)];
        MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
        DBGLOG("hwlibs", "Applied DDI Caps patches");

        return true;
    }

    return false;
}

void HWLibs::wrapAmdCailServicesConstructor(void *that, IOPCIDevice *provider) {
    DBGLOG("hwlibs", "AmdCailServices constructor called!");
    FunctionCast(wrapAmdCailServicesConstructor, callback->orgAmdCailServicesConstructor)(that, provider);
}

void *HWLibs::wrapCreatePowerTuneServices(void *that, void *param2) {
    auto *ret = operator new(0x18);
    (LRed::callback->isGCN3 ? callback->orgTongaPowerTuneConstructor : callback->orgHawaiiPowerTuneConstructor)(ret,
        that, param2);
    return ret;
}

uint64_t HWLibs::wrapSMUMInitialize(uint64_t param1, uint32_t *param2, uint64_t param3) {
    DBGLOG("hwlibs", "_SMUM_Initialize: param1 = 0x%llX param2 = %p param3 = 0x%llX", param1, param2, param3);
    auto ret = FunctionCast(wrapSMUMInitialize, callback->orgSMUMInitialize)(param1, param2, param3);
    DBGLOG("hwlibs", "_SMUM_Initialize returned 0x%llX", ret);
    return ret;
}
/** Modifications made, highly unlikely to work, will change later on */
void HWLibs::wrapMCILDebugPrint(uint32_t level_max, char *fmt, uint64_t param3, uint64_t param4, uint64_t param5,
    uint level) {
    printf("_MCILDebugPrint PARAM1 = 0x%X: ", level_max);
    printf(fmt, param3, param4, param5, level);
    FunctionCast(wrapMCILDebugPrint, callback->orgMCILDebugPrint)(level_max, fmt, param3, param4, param5, level);
}

/** For future reference */
/*
AMDReturn X5000HWLibs::wrapSmuRavenInitialize(void *smum, uint32_t param2) {
    auto ret = FunctionCast(wrapSmuRavenInitialize, callback->orgSmuRavenInitialize)(smum, param2);
    callback->orgRavenSendMsgToSmc(smum, PPSMC_MSG_PowerUpSdma);
    return ret;
}
*/
