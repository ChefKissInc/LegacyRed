//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.5. See LICENSE for
//  details.

#ifndef kern_hwlibs_hpp
#define kern_hwlibs_hpp
#include "kern_amd.hpp"
#include "kern_lred.hpp"
#include "kern_patcherplus.hpp"
#include <Headers/kern_util.hpp>

using t_XPowerTuneConstructor = void (*)(void *that, void *ppInstance, void *ppCallbacks);
using t_sendMsgToSmc = uint32_t (*)(void *smum, uint32_t msgId);

class HWLibs {
    public:
    static HWLibs *callback;
    void init();
    bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    private:

    t_XPowerTuneConstructor orgHawaiiPowerTuneConstructor {nullptr};
    t_XPowerTuneConstructor orgTongaPowerTuneConstructor {nullptr};
    t_sendMsgToSmc orgCISendMsgToSmc {nullptr};
    t_sendMsgToSmc orgCzSendMsgToSmc {nullptr};
    mach_vm_address_t orgAmdCailServicesConstructor {0};
    mach_vm_address_t orgCAILQueryEngineRunningState {0};
    mach_vm_address_t orgCailMonitorEngineInternalState {0};
    mach_vm_address_t orgCailMonitorPerformanceCounter {0};
    mach_vm_address_t orgSMUMInitialize {0};
    mach_vm_address_t orgSmuCzInitialize {0};
    mach_vm_address_t orgMCILDebugPrint {0};
    mach_vm_address_t orgCailBonaireLoadUcode {0};
    mach_vm_address_t orgBonaireLoadUcodeViaPortRegister {0};
    mach_vm_address_t orgBonaireProgramAspm {0};

    static void wrapAmdCailServicesConstructor(void *that, IOPCIDevice *provider);
    static uint64_t wrapCAILQueryEngineRunningState(void *param1, uint32_t *param2, uint64_t param3);
    static uint64_t wrapCailMonitorEngineInternalState(void *that, uint32_t param1, uint32_t *param2);
    static uint64_t wrapCailMonitorPerformanceCounter(void *that, uint32_t *param1);
    static void *wrapCreatePowerTuneServices(void *that, void *param2);
    static AMDReturn wrapSmuCzInitialize(void *smum, uint32_t param2);
    static uint64_t wrapSMUMInitialize(uint64_t param1, uint32_t *param2, uint64_t param3);
    static void wrapMCILDebugPrint(uint32_t level_max, char *fmt, uint64_t param3, uint64_t param4, uint64_t param5,
        uint level);
    static void wrapCailBonaireLoadUcode(uint64_t param1, uint64_t param2, uint64_t param3, uint64_t param4);
    static void wrapBonaireLoadUcodeViaPortRegister(uint64_t param1, uint64_t param2, void *param3, uint32_t param4,
        uint32_t param5);
    static uint64_t wrapBonaireProgramAspm(uint64_t param1);
};

#endif /* kern_hwlibs_hpp */
