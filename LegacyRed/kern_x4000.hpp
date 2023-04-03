//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_x4000_hpp
#define kern_x4000_hpp
#include "kern_amd.hpp"
#include <Headers/kern_patcher.hpp>
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>

class X4000 {
    public:
    static X4000 *callback;
    void init();
    bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    private:
    t_GenericConstructor orgGFX7PM4EngineConstructor {nullptr};
    t_GenericConstructor orgGFX7SDMAEngineConstructor {nullptr};
    t_GenericConstructor orgGFX7VCEEngineConstructor {nullptr};
    t_GenericConstructor orgGFX7UVDEngineConstructor {nullptr};
    t_GenericConstructor orgGFX7SAMUEngineConstructor {nullptr};
    t_GenericConstructor orgGFX8PM4EngineConstructor {nullptr};
    t_GenericConstructor orgGFX8SDMAEngineConstructor {nullptr};
    t_GenericConstructor orgGFX8VCEEngineConstructor {nullptr};
    t_GenericConstructor orgGFX8UVDEngineConstructor {nullptr};
    t_GenericConstructor orgGFX8SAMUEngineConstructor {nullptr};
    mach_vm_address_t orgSetupAndInitializeHWCapabilities {0};
    mach_vm_address_t orgAccelStart {};

	static bool wrapAllocateHWEngines(void *that);
	static void wrapSetupAndInitializeHWCapabilities(void *that);
	static void wrapInitializeFamilyType(void *that);
    void *callbackAccelerator = nullptr;
    static bool wrapAccelStart(void *that, IOService *provider);
};

#endif /* kern_x4000_hpp */
