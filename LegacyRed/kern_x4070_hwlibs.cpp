//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_x4070_hwlibs.hpp"
#include "kern_lred.hpp"
#include "kern_patches.hpp"
#include <Headers/kern_api.hpp>

static const char *pathRadeonX4070HWLibs = "/System/Library/Extensions/AMDRadeonX4000HWServices.kext/Contents/PlugIns/"
                                           "AMDRadeonX4070HWLibs.kext/Contents/MacOS/AMDRadeonX4070HWLibs";

static KernelPatcher::KextInfo kextRadeonX4070HWLibs {"com.apple.kext.AMDRadeonX4070HWLibs", &pathRadeonX4070HWLibs, 1,
    {}, {}, KernelPatcher::KextInfo::Unloaded};

X4070HWLibs *X4070HWLibs::callback = nullptr;

void X4070HWLibs::init() {
    callback = this;
    lilu.onKextLoadForce(&kextRadeonX4070HWLibs);
}

bool X4070HWLibs::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextRadeonX4070HWLibs.loadIndex == index) {
        LRed::callback->setRMMIOIfNecessary();

        CailAsicCapEntry *orgAsicCapsTable = nullptr;
        CailInitAsicCapEntry *orgAsicInitCapsTable = nullptr;
        const void *goldenSettings[static_cast<uint32_t>(ChipType::Unknown)] = {nullptr};
        const uint32_t *ddiCaps[static_cast<uint32_t>(ChipType::Unknown)] = {nullptr};

        /** The Bristol Ridge else if is a complete shot in the dark, it is not guaranteed it will work */
        if (LRed::callback->chipVariant == ChipVariant::s3CU) {
            KernelPatcher::SolveRequest solveRequests[] = {
                {"_STONEY_GoldenSettings_A0_3CU", goldenSettings[static_cast<uint32_t>(ChipType::Stoney)]},
            };
            PANIC_COND(!patcher.solveMultiple(index, solveRequests, address, size), "hwlibs",
                "Failed to resolve symbols");
            DBGLOG("hwlibs", "Using 3CU Stoney Golden Settings");
        } else if (LRed::callback->chipVariant == ChipVariant::s2CU) {
            KernelPatcher::SolveRequest solveRequests[] = {
                {"_STONEY_GoldenSettings_A0_2CU", goldenSettings[static_cast<uint32_t>(ChipType::Stoney)]},
            };
            PANIC_COND(!patcher.solveMultiple(index, solveRequests, address, size), "hwlibs",
                "Failed to resolve symbols");
            DBGLOG("hwlibs", "Using 2CU Stoney Golden Settings");
        } else if (LRed::callback->chipVariant == ChipVariant::Bristol) {
            KernelPatcher::SolveRequest solveRequests[] = {
                {"_CARRIZO_GoldenSettings_A1", goldenSettings[static_cast<uint32_t>(ChipType::Carrizo)]},
                {"_CAIL_DDI_CAPS_CARRIZO_A1", ddiCaps[static_cast<uint32_t>(ChipType::Carrizo)]},
            };
            PANIC_COND(!patcher.solveMultiple(index, solveRequests, address, size), "hwlibs",
                "Failed to resolve symbols");
            DBGLOG("hwlibs", "Using A1 Carrizo Golden Settings and DDI Caps");
        } else {
            KernelPatcher::SolveRequest solveRequests[] = {
                {"_CARRIZO_GoldenSettings_A0", goldenSettings[static_cast<uint32_t>(ChipType::Carrizo)]},
                {"_CAIL_DDI_CAPS_CARRIZO_A0", ddiCaps[static_cast<uint32_t>(ChipType::Carrizo)]},
            };
            PANIC_COND(!patcher.solveMultiple(index, solveRequests, address, size), "hwlibs",
                "Failed to resolve symbols");
            DBGLOG("hwlibs", "Using A0 Carrizo Golden Settings and DDI Caps");
        }

        KernelPatcher::SolveRequest solveRequests[] = {
            {"__ZN30AtiAppleTongaPowerTuneServicesC1EP11PP_InstanceP18PowerPlayCallbacks",
                this->orgTongaPowerTuneConstructor},
            {"__ZL20CAIL_ASIC_CAPS_TABLE", orgAsicCapsTable}, {"_CAILAsicCapsInitTable", orgAsicInitCapsTable},
            //{"_Raven_SendMsgToSmc", this->orgRavenSendMsgToSmc},
        };
        PANIC_COND(!patcher.solveMultiple(index, solveRequests, address, size), "hwlibs", "Failed to resolve symbols");

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

void X4070HWLibs::wrapAmdCailServicesConstructor(void *that, IOPCIDevice *provider) {
    DBGLOG("lred", "AmdCailServices constructor called!");
    FunctionCast(wrapAmdCailServicesConstructor, callback->orgAmdCailServicesConstructor)(that, provider);
}

void *X4070HWLibs::wrapCreatePowerTuneServices(void *that, void *param2) {
    auto *ret = IOMallocZero(0x18);
    callback->orgTongaPowerTuneConstructor(ret, that, param2);
    return ret;
}

uint64_t X4070HWLibs::wrapSMUMInitialize(uint64_t param1, uint32_t *param2, uint64_t param3) {
    DBGLOG("hwlibs", "_SMUM_Initialize: param1 = 0x%llX param2 = %p param3 = 0x%llX", param1, param2, param3);
    auto ret = FunctionCast(wrapSMUMInitialize, callback->orgSMUMInitialize)(param1, param2, param3);
    DBGLOG("hwlibs", "_SMUM_Initialize returned 0x%llX", ret);
    return ret;
}
/** Modifications made, highly unlikely to work, will change later on */
void X4070HWLibs::wrapMCILDebugPrint(uint32_t level_max, char *fmt, uint64_t param3, uint64_t param4, uint64_t param5,
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
