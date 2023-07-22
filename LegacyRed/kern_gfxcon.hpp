//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_gfxcon_hpp
#define kern_gfxcon_hpp
#include "kern_amd.hpp"
#include "kern_patcherplus.hpp"
#include <Headers/kern_util.hpp>

class GFXCon {
    public:
    static GFXCon *callback;
    void init();
    bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    private:
    mach_vm_address_t orgHwReadReg8 {0};
    mach_vm_address_t orgHwReadReg16 {0};
    mach_vm_address_t orgHwReadReg32 {0};
    mach_vm_address_t orgPopulateDeviceInfo {0};

    static IOReturn wrapPopulateDeviceInfo(void *that);
    static uint8_t wrapHwReadReg8(void *that, uint8_t param1);
    static uint16_t wrapHwReadReg16(void *that, uint16_t param1);
    static uint32_t wrapHwReadReg32(void *that, uint32_t param1);
};

#endif /* kern_gfxcon_hpp */
