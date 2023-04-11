//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_gfx8con_hpp
#define kern_gfx8con_hpp
#include "kern_amd.hpp"
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_util.hpp>

class GFX8Con {
    public:
    static GFX8Con *callback;
    void init();
    bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    private:
    mach_vm_address_t orgGetFamilyId {};
    mach_vm_address_t orgHwReadReg32 {0};
    mach_vm_address_t orgPopulateDeviceInfo {0};

    static IOReturn wrapPopulateDeviceInfo(void *that);
    static uint16_t wrapGetFamilyId(void);
    static uint32_t wrapHwReadReg32(void *that, uint32_t param1);
};

#endif /* kern_gfx8con_hpp */
