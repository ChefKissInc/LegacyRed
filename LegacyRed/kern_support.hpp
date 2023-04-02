//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

//  Copyright © 2023 Zormeister. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_support_hpp
#define kern_support_hpp
#include "kern_amd.hpp"
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>

class Support {
    public:
    static Support *callback;
    void init();
    bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    private:
    mach_vm_address_t orgHwReadReg32 {0};
	mach_vm_address_t orgPopulateDeviceMemory {};

	static bool wrapPopulateDeviceMemory(IOService *that, uint32_t param_1);
    static uint32_t wrapHwReadReg32(void *that, uint32_t param1);
};

#endif /* kern_support_hpp */
