//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_x4000_hpp
#define kern_x4000_hpp
#include "kern_amd.hpp"
#include "kern_lred.hpp"
#include "kern_patcherplus.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>

class X4000 {
    public:
    static X4000 *callback;
    void init();
    bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    private:
    t_GenericConstructor orgBaffinPM4EngineConstructor {nullptr};
    t_GenericConstructor orgGFX8SDMAEngineConstructor {nullptr};
    t_GenericConstructor orgPolarisVCEEngineConstructor {nullptr};
    t_GenericConstructor orgPolarisUVDEngineConstructor {nullptr};
    t_GenericConstructor orgGFX8SAMUEngineConstructor {nullptr};
    mach_vm_address_t orgAccelStart {0};
    mach_vm_address_t orgConfigureDevice {0};
    mach_vm_address_t orgCreateHWHandler {0};
    mach_vm_address_t orgCreateHWInterface {0};
    mach_vm_address_t orgGetHWChannel {0};
    mach_vm_address_t orgInitLinkToPeer {0};
    mach_vm_address_t orgSetupAndInitializeHWCapabilities {0};

    void *callbackAccelerator = nullptr;

    static bool wrapAccelStart(void *that, IOService *provider);
    static uint64_t wrapConfigureDevice(void *that, IOPCIDevice *device);
    static void *wrapGetHWChannel(void *that, uint32_t engineType, uint32_t ringId);
    static void wrapInitializeFamilyType(void *that);
    static IOService *wrapInitLinkToPeer(void *that, const char *matchCategoryName);
    static uint64_t wrapCreateHWHandler(void *that);
    static uint64_t wrapCreateHWInterface(void *that, IOPCIDevice *dev);
    static void wrapSetupAndInitializeHWCapabilities(void *that);
    static char *forceX4000HWLibs(void);
};

#endif /* kern_x4000_hpp */
