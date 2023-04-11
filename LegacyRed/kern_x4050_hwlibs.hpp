//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_x4050_hwlibs_hpp
#define kern_x4050_hwlibs_hpp
#include "kern_amd.hpp"
#include "kern_lred.hpp"
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_util.hpp>

using t_XPowerTuneConstructor = void (*)(void *that, void *ppInstance, void *ppCallbacks);
using t_sendMsgToSmc = uint32_t (*)(void *smum, uint32_t msgId);

class X4050HWLibs {
    public:
    static X4050HWLibs *callback;
    void init();
    bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    private:
    t_XPowerTuneConstructor orgHawaiiPowerTuneConstructor {nullptr};
    t_sendMsgToSmc orgRavenSendMsgToSmc {nullptr};
    mach_vm_address_t orgAmdCailServicesConstructor {};
    mach_vm_address_t orgCAILQueryEngineRunningState {};
    mach_vm_address_t orgCailMonitorEngineInternalState {};
    mach_vm_address_t orgCailMonitorPerformanceCounter {};
    mach_vm_address_t orgSMUMInitialize {};
    mach_vm_address_t orgSmuRavenInitialize {0};
    mach_vm_address_t orgMCILDebugPrint {};

    static void wrapAmdCailServicesConstructor(void *that, IOPCIDevice *provider);
    static uint64_t wrapCAILQueryEngineRunningState(void *param1, uint32_t *param2, uint64_t param3);
    static uint64_t wrapCailMonitorEngineInternalState(void *that, uint32_t param1, uint32_t *param2);
    static uint64_t wrapCailMonitorPerformanceCounter(void *that, uint32_t *param1);
    static void *wrapCreatePowerTuneServices(void *that, void *param2);
    static AMDReturn wrapSmuRavenInitialize(void *smum, uint32_t param2);
    static uint64_t wrapSMUMInitialize(uint64_t param1, uint32_t *param2, uint64_t param3);
    static void wrapMCILDebugPrint(uint32_t level_max, char *fmt, uint64_t param3, uint64_t param4, uint64_t param5,
        uint level);
};

#endif /* kern_x4050_hwlibs_hpp */
