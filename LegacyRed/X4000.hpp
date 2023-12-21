//! Copyright © 2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.5.
//! See LICENSE for details.

#pragma once
#include "AMDCommon.hpp"
#include "LRed.hpp"
#include "PatcherPlus.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>

class X4000 {
    public:
    static X4000 *callback;
    void init();
    bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    private:
    mach_vm_address_t orgAccelStart {0};
    mach_vm_address_t orgGetHWChannel {0};
    mach_vm_address_t orgSetupAndInitializeHWCapabilities {0};
    mach_vm_address_t orgAdjustVRAMAddress {0};
    mach_vm_address_t orgInitializeMicroEngine {0};
    mach_vm_address_t orgInitializeVMRegs {0};
    mach_vm_address_t orgHwlInitGlobalParams {0};
    mach_vm_address_t orgGetHWInfo = {0};
    mach_vm_address_t orgAMDSMLUVDInit = {0};
    mach_vm_address_t orgAMDSMLVCEInit = {0};
    mach_vm_address_t orgAMDHWRegsWrite {0};

    void *callbackAccelerator = nullptr;

    static bool wrapAccelStart(void *that, IOService *provider);
    static void *wrapGetHWChannel(void *that, UInt32 engineType, UInt32 ringId);
    static void wrapInitializeFamilyType(void *that);
    static void wrapSetupAndInitializeHWCapabilities(void *that);
    static void wrapDumpASICHangState();
    static UInt64 wrapAdjustVRAMAddress(void *that, UInt64 addr);
    static bool wrapInitializeMicroEngine(void *that);
    static void wrapInitializeVMRegs(void *that);
    static int wrapHwlInitGlobalParams(void *that, const void *creationInfo);
    static IOReturn wrapGetHWInfo(void *ctx, void *hwInfo);
    static void wrapAMDHWRegsWrite(void *that, UInt32 addr, UInt32 val);

    static bool wrapAMDSMLUVDInit(void *that);
    static bool wrapAMDSMLVCEInit(void *that);
};

// ---- Patches ---- //

//! makes a loop run once in startHWEngines
static const UInt8 kStartHWEnginesOriginal[] = {0x40, 0x83, 0xF0, 0x02};
static const UInt8 kStartHWEnginesMask[] = {0xF0, 0xFF, 0xF0, 0xFF};
static const UInt8 kStartHWEnginesPatched[] = {0x40, 0x83, 0xF0, 0x01};

//! erase the 2nd SDMA engine. this is a viable method for us since we don't have VCN or any other component found in
//! the other kexts
static const UInt8 kAMDEllesmereHWallocHWEnginesOriginal[] = {0xE8, 0xAA, 0xF3, 0xFE, 0xFF, 0x48, 0x89, 0xC3, 0x48,
    0x89, 0xC7, 0xE8, 0xA9, 0xF3, 0xFE, 0xFF, 0x49, 0x89, 0x9E, 0xC0, 0x03, 0x00, 0x00};
static const UInt8 kAMDEllesmereHWallocHWEnginesPatched[] = {0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90,
    0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x90};

// ---- Patterns ---- //

//! Due to how generic the function is, it's almost impossible to use a mask without hitting some other SML VCE/UVD
//! class, also this is literally the whole function
static const UInt8 kAMDVCE3v4InitBigSur[] = {0x55, 0x48, 0x89, 0xe5, 0x53, 0x50, 0x48, 0x89, 0xfb, 0x48, 0x8d, 0x05,
    0x62, 0x24, 0x35, 0x00, 0xff, 0x90, 0x28, 0x01, 0x00, 0x00, 0x84, 0xc0, 0x74, 0x21, 0xc7, 0x43, 0x14, 0x60, 0x8b,
    0x02, 0x00, 0x48, 0x8d, 0x05, 0x52, 0xec, 0x1e, 0x00, 0x48, 0x89, 0x43, 0x18, 0xc7, 0x43, 0x24, 0x01, 0x00, 0x00,
    0x00, 0xc6, 0x43, 0x20, 0x01, 0xb0, 0x01, 0xeb, 0x02, 0x31, 0xc0, 0x48, 0x83, 0xc4, 0x08, 0x5b, 0x5d, 0xc3};
static const UInt8 kAMDUVD6v3InitBigSur[] = {0x55, 0x48, 0x89, 0xe5, 0x53, 0x50, 0x48, 0x89, 0xfb, 0x48, 0x8d, 0x05,
    0x14, 0x79, 0x35, 0x00, 0xff, 0x90, 0x28, 0x01, 0x00, 0x00, 0x84, 0xc0, 0x74, 0x1a, 0xc7, 0x43, 0x2c, 0x38, 0x19,
    0x06, 0x00, 0x48, 0x8d, 0x05, 0x84, 0xbf, 0x13, 0x00, 0x48, 0x89, 0x43, 0x30, 0xc6, 0x43, 0x28, 0x01, 0xb0, 0x01,
    0xeb, 0x02, 0x31, 0xc0, 0x48, 0x83, 0xc4, 0x08, 0x5b, 0x5d, 0xc3, 0x90};
static const UInt8 kAMDVIVCEInitBigSur[] = {0x55, 0x48, 0x89, 0xe5, 0x41, 0x56, 0x53, 0x41, 0x89, 0xf6, 0x48, 0x89,
    0xfb, 0x48, 0x8d, 0x05, 0xc0, 0x26, 0x35, 0x00, 0xff, 0x90, 0x28, 0x01, 0x00, 0x00, 0x84, 0xc0, 0x74, 0x2f, 0xc7,
    0x43, 0x14, 0xc0, 0xb4, 0x02, 0x00, 0x48, 0x8d, 0x05, 0xf0, 0x39, 0x1c, 0x00, 0x48, 0x89, 0x43, 0x18, 0xb0, 0x01,
    0x41, 0x81, 0xfe, 0x00, 0x00, 0x00, 0x02, 0x74, 0x16, 0x41, 0x81, 0xfe, 0x00, 0x00, 0x00, 0x01, 0x75, 0x14, 0xc7,
    0x43, 0x24, 0x02, 0x00, 0x00, 0x00, 0xeb, 0x0b, 0x31, 0xc0, 0xeb, 0x07, 0xc7, 0x43, 0x24, 0x01, 0x00, 0x00, 0x00,
    0x5b, 0x41, 0x5e, 0x5d, 0xc3, 0x90};
static const UInt8 kAMDUVDVIInitBigSur[] = {0x55, 0x48, 0x89, 0xe5, 0x53, 0x50, 0x48, 0x89, 0xfb, 0x48, 0x8d, 0x05,
    0x2a, 0x79, 0x35, 0x00, 0xff, 0x90, 0x28, 0x01, 0x00, 0x00, 0x84, 0xc0, 0x74, 0x25, 0xc7, 0x43, 0x2c, 0xd4, 0xf8,
    0x04, 0x00, 0x48, 0x8d, 0x05, 0x52, 0xc9, 0x0e, 0x00, 0x48, 0x89, 0x43, 0x30, 0xc6, 0x43, 0x28, 0x01, 0xc6, 0x43,
    0x40, 0x00, 0xc7, 0x43, 0x44, 0x01, 0x00, 0x00, 0x00, 0xb0, 0x01, 0xeb, 0x02, 0x31, 0xc0, 0x48, 0x83, 0xc4, 0x08,
    0x5b, 0x5d, 0xc3};