//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_hwlibs.hpp"
#include "kern_lred.hpp"
#include "kern_patches.hpp"
#include <Headers/kern_api.hpp>

static const char *pathRadeonX4000HWLibs = "/System/Library/Extensions/AMDRadeonX4000HWServices.kext/Contents/PlugIns/"
                                           "AMDRadeonX4000HWLibs.kext/Contents/MacOS/AMDRadeonX4000HWLibs";

static KernelPatcher::KextInfo kextRadeonX4000HWLibs {"com.apple.kext.AMDRadeonX4000HWLibs", &pathRadeonX4000HWLibs, 1,
    {}, {}, KernelPatcher::KextInfo::Unloaded};

HWLibs *HWLibs::callback = nullptr;

void HWLibs::init() {
    callback = this;
    lilu.onKextLoadForce(&kextRadeonX4000HWLibs);
}

bool HWLibs::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextRadeonX4000HWLibs.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();

        CailAsicCapEntry *orgAsicCapsTable = nullptr;
        CailInitAsicCapEntry *orgAsicInitCapsTable = nullptr;
        const void *goldenSettings[static_cast<uint32_t>(ChipType::Unknown)] = {nullptr};
        const uint32_t *ddiCaps[static_cast<uint32_t>(ChipType::Unknown)] = {nullptr};

        // The pains of supporting more than two iGPU generations
        switch (LRed::callback->chipVariant) {
            case ChipVariant::s2CU: {
                SolveRequestPlus solveRequests[] = {
                    {"_STONEY_GoldenSettings_A0_2CU", goldenSettings[static_cast<uint32_t>(ChipType::Stoney)]},
                    {"_CAIL_DDI_CAPS_STONEY_A0", ddiCaps[static_cast<uint32_t>(ChipType::Stoney)]},
                };
                PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
                    "Failed to resolve symbols");
                DBGLOG("hwlibs", "Stoney ASIC is 2CU model");
                break;
            }
            case ChipVariant::s3CU: {
                SolveRequestPlus solveRequests[] = {
                    {"_STONEY_GoldenSettings_A0_3CU", goldenSettings[static_cast<uint32_t>(ChipType::Stoney)]},
                    {"_CAIL_DDI_CAPS_STONEY_A0", ddiCaps[static_cast<uint32_t>(ChipType::Stoney)]},
                };
                PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
                    "Failed to resolve symbols");
                DBGLOG("hwlibs", "Stoney ASIC is 3CU model");
                break;
            }
            case ChipVariant::Bristol: {
                SolveRequestPlus solveRequests[] = {
                    {"_CARRIZO_GoldenSettings_A1", goldenSettings[static_cast<uint32_t>(ChipType::Carrizo)]},
                    {"_CAIL_DDI_CAPS_CARRIZO_A1", ddiCaps[static_cast<uint32_t>(ChipType::Carrizo)]},
                };
                PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
                    "Failed to resolve symbols");
                DBGLOG("hwlibs", "Carrizo ASIC is Bristol Ridge model");
                break;
            }
            default: {
                SolveRequestPlus solveRequests[] = {
                    {"_CAIL_DDI_CAPS_SPECTRE_A0", ddiCaps[static_cast<uint32_t>(ChipType::Spectre)]},
                    {"_SPECTRE_GoldenSettings_A0_8812", goldenSettings[static_cast<uint32_t>(ChipType::Spectre)]},
                    {"_CAIL_DDI_CAPS_SPECTRE_A0", ddiCaps[static_cast<uint32_t>(ChipType::Spooky)]},
                    {"_SPECTRE_GoldenSettings_A0_8812", goldenSettings[static_cast<uint32_t>(ChipType::Spooky)]},
                    {"_CAIL_DDI_CAPS_KALINDI_A0", ddiCaps[static_cast<uint32_t>(ChipType::Kalindi)]},
                    {"_KALINDI_GoldenSettings_A0_4882", goldenSettings[static_cast<uint32_t>(ChipType::Kalindi)]},
                    {"_CAIL_DDI_CAPS_KALINDI_A1", ddiCaps[static_cast<uint32_t>(ChipType::Godavari)]},
                    {"_GODAVARI_GoldenSettings_A0_2411", goldenSettings[static_cast<uint32_t>(ChipType::Godavari)]},
                    {"_CARRIZO_GoldenSettings_A0", goldenSettings[static_cast<uint32_t>(ChipType::Carrizo)]},
                    {"_CAIL_DDI_CAPS_CARRIZO_A0", ddiCaps[static_cast<uint32_t>(ChipType::Carrizo)]},
                    /** Spectre appears to be another name for Kaveri, so that's the logic we'll use for it */
                    // Spooky has no DDI caps or GoldenSettings, uses Spectre on AMDGPU so that's what we'll use
                };
                PANIC_COND(!SolveRequestPlus::solveAll(&patcher, index, solveRequests, address, size), "hwlibs",
                    "Failed to resolve symbols");
                DBGLOG("hwlibs", "Using Normal Golden Settings and DDI Caps");
                break;
            }
        }

        SolveRequestPlus solveRequests[] = {
            {"__ZN31AtiAppleHawaiiPowerTuneServicesC1EP11PP_InstanceP18PowerPlayCallbacks",
                this->orgHawaiiPowerTuneConstructor, !LRed::callback->isGCN3},
            {"__ZN30AtiAppleTongaPowerTuneServicesC1EP11PP_InstanceP18PowerPlayCallbacks",
                this->orgTongaPowerTuneConstructor, LRed::callback->isGCN3},
            {"__ZL20CAIL_ASIC_CAPS_TABLE", orgAsicCapsTable},
            {"_CAILAsicCapsInitTable", orgAsicInitCapsTable},
            {"_CIslands_SendMsgToSmc", this->orgCISendMsgToSmc, LRed::callback->chipType < ChipType::Carrizo},
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
            {"_CailMonitorEngineInternalState", wrapCailMonitorEngineInternalState, orgCailMonitorEngineInternalState},
            {"_MCILDebugPrint", wrapMCILDebugPrint, orgMCILDebugPrint},
            {"_SMUM_Initialize", wrapSMUMInitialize, orgSMUMInitialize},
            //{"_SmuCz_Initialize", wrapSmuCzInitialize, this->orgSmuCzInitialize},
        };
        PANIC_COND(!patcher.routeMultiple(index, requests, address, size), "hwlibs", "Failed to route symbols");

        PANIC_COND(MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) != KERN_SUCCESS, "hwlibs",
            "Failed to enable kernel writing");
        orgAsicInitCapsTable->familyId = orgAsicCapsTable->familyId =
            LRed::callback->isGCN3 ? AMDGPU_FAMILY_CZ : AMDGPU_FAMILY_KV;
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

uint64_t HWLibs::wrapCAILQueryEngineRunningState(void *param1, uint32_t *param2, uint64_t param3) {
    DBGLOG("hwlibs", "_CAILQueryEngineRunningState: param1 = %p param2 = %p param3 = %llX", param1, param2, param3);
    DBGLOG("hwlibs", "_CAILQueryEngineRunningState: *param2 = 0x%X", *param2);
    auto ret =
        FunctionCast(wrapCAILQueryEngineRunningState, callback->orgCAILQueryEngineRunningState)(param1, param2, param3);
    DBGLOG("hwlibs", "_CAILQueryEngineRunningState: after *param2 = 0x%X", *param2);
    DBGLOG("hwlibs", "_CAILQueryEngineRunningState returned 0x%llX", ret);
    return ret;
}

uint64_t HWLibs::wrapCailMonitorEngineInternalState(void *that, uint32_t param1, uint32_t *param2) {
    DBGLOG("hwlibs", "_CailMonitorEngineInternalState: this = %p param1 = 0x%X param2 = %p", that, param1, param2);
    DBGLOG("hwlibs", "_CailMonitorEngineInternalState: *param2 = 0x%X", *param2);
    auto ret = FunctionCast(wrapCailMonitorEngineInternalState, callback->orgCailMonitorEngineInternalState)(that,
        param1, param2);
    DBGLOG("hwlibs", "_CailMonitorEngineInternalState: after *param2 = 0x%X", *param2);
    DBGLOG("hwlibs", "_CailMonitorEngineInternalState returned 0x%llX", ret);
    return ret;
}

uint64_t HWLibs::wrapCailMonitorPerformanceCounter(void *that, uint32_t *param1) {
    DBGLOG("hwlibs", "_CailMonitorPerformanceCounter: this = %p param1 = %p", that, param1);
    DBGLOG("hwlibs", "_CailMonitorPerformanceCounter: *param1 = 0x%X", *param1);
    auto ret =
        FunctionCast(wrapCailMonitorPerformanceCounter, callback->orgCailMonitorPerformanceCounter)(that, param1);
    DBGLOG("hwlibs", "_CailMonitorPerformanceCounter: after *param1 = 0x%X", *param1);
    DBGLOG("hwlibs", "_CailMonitorPerformanceCounter returned 0x%llX", ret);
    return ret;
}

/** For future reference */
/*
AMDReturn X5000HWLibs::wrapSmuRavenInitialize(void *smum, uint32_t param2) {
    auto ret = FunctionCast(wrapSmuRavenInitialize, callback->orgSmuRavenInitialize)(smum, param2);
    callback->orgRavenSendMsgToSmc(smum, PPSMC_MSG_PowerUpSdma);
    return ret;
}
*/
