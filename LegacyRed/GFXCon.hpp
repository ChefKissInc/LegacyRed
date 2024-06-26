//! Copyright © 2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.5.
//! See LICENSE for details.

#pragma once
#include "AMDCommon.hpp"
#include "PatcherPlus.hpp"
#include <Headers/kern_util.hpp>

using t_SetRBReadPointer = void (*)(void *that, UInt32 addr);
using t_GetActiveRingBufferSizeRegValue = UInt32 (*)(void *that);
using t_GetWPTRWriteBackOffset = long (*)(void *that);
using t_IsUsingVRAMForRingBuffer = UInt32 (*)(void *that);

class GFXCon {
    public:
    static GFXCon *callback;
    void init();
    bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    private:
    mach_vm_address_t orgPopulateDeviceInfo {0};
    mach_vm_address_t orgGetFamilyId {0};
    t_SetRBReadPointer IHSetRBReadPointer;
    t_GetWPTRWriteBackOffset IHGetWPTRWriteBackOffset;
    t_GetActiveRingBufferSizeRegValue IHGetActiveRingBufferSizeRegValue;
    t_IsUsingVRAMForRingBuffer IHIsUsingVRAMForRingBuffer;

    static IOReturn wrapPopulateDeviceInfo(void *that);
    static UInt16 wrapGetFamilyId(void);

    static void IHSetHardwareEnabled(void *that, bool enabled);
};
