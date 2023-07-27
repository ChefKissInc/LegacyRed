//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_support_hpp
#define kern_support_hpp
#include "kern_amd.hpp"
#include "kern_patcherplus.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>

// Taken from WhateverGreen's kern_agdc.h, used for a wrap in kern_support.cpp
enum kAGDCRegisterLinkControlEvent_t {
    kAGDCRegisterLinkInsert = 0,
    kAGDCRegisterLinkRemove = 1,
    kAGDCRegisterLinkChange = 2,
    kAGDCRegisterLinkChangeMST = 3,
    kAGDCRegisterLinkFramebuffer = 4,
    kAGDCValidateDetailedTiming = 10,
    kAGDCRegisterLinkChangeWakeProbe = 0x80,
};

struct AGDCDetailedTimingInformation_t {
    uint32_t horizontalScaledInset;    // pixels
    uint32_t verticalScaledInset;      // lines

    uint32_t scalerFlags;
    uint32_t horizontalScaled;
    uint32_t verticalScaled;

    uint32_t signalConfig;
    uint32_t signalLevels;

    uint64_t pixelClock;    // Hz

    uint64_t minPixelClock;    // Hz - With error what is slowest actual clock
    uint64_t maxPixelClock;    // Hz - With error what is fasted actual clock

    uint32_t horizontalActive;            // pixels
    uint32_t horizontalBlanking;          // pixels
    uint32_t horizontalSyncOffset;        // pixels
    uint32_t horizontalSyncPulseWidth;    // pixels

    uint32_t verticalActive;            // lines
    uint32_t verticalBlanking;          // lines
    uint32_t verticalSyncOffset;        // lines
    uint32_t verticalSyncPulseWidth;    // lines

    uint32_t horizontalBorderLeft;     // pixels
    uint32_t horizontalBorderRight;    // pixels
    uint32_t verticalBorderTop;        // lines
    uint32_t verticalBorderBottom;     // lines

    uint32_t horizontalSyncConfig;
    uint32_t horizontalSyncLevel;    // Future use (init to 0)
    uint32_t verticalSyncConfig;
    uint32_t verticalSyncLevel;    // Future use (init to 0)
    uint32_t numLinks;
    uint32_t verticalBlankingExtension;
    uint16_t pixelEncoding;
    uint16_t bitsPerColorComponent;
    uint16_t colorimetry;
    uint16_t dynamicRange;
    uint16_t dscCompressedBitsPerPixel;
    uint16_t dscSliceHeight;
    uint16_t dscSliceWidth;
};

struct AGDCValidateDetailedTiming_t {
    uint32_t framebufferIndex;    // IOFBDependentIndex
    AGDCDetailedTimingInformation_t timing;
    uint16_t padding1[5];
    void *cfgInfo;    // AppleGraphicsDevicePolicy configuration
    int32_t frequency;
    uint16_t padding2[6];
    uint32_t modeStatus;    // 1 - invalid, 2 - success, 3 - change timing
    uint16_t padding3[2];
};

struct AtomConnectorInfo;    // Needs more reversing, here as a place holder
struct BiosParserServices;
struct DCE_Version {
    uint32_t dceVersion;
};
class AtiBiosParser1;

class Support {
    public:
    static Support *callback;
    void init();
    bool processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    private:
    mach_vm_address_t orgPopulateDeviceMemory {0};
    mach_vm_address_t orgNotifyLinkChange {0};
    mach_vm_address_t orgGetAtomConnectorInfo {0};
    mach_vm_address_t orgGetNumberOfConnectors {0};
    mach_vm_address_t orgCreateAtomBiosParser {0};

    static bool wrapNotifyLinkChange(void *atiDeviceControl, kAGDCRegisterLinkControlEvent_t event, void *eventData,
        uint32_t eventFlags);
    static bool doNotTestVram(IOService *ctrl, uint32_t reg, bool retryOnFail);
    static IOReturn wrapPopulateDeviceMemory(void *that, uint32_t reg);
    static IOReturn wrapGetAtomConnectorInfo(void *that, uint32_t connector, AtomConnectorInfo *coninfo);
    static uint32_t wrapGetNumberOfConnectors(void *that);
    static AtiBiosParser1 *wrapCreateAtomBiosParser(void *that, BiosParserServices *param1, unsigned char *param2, DCE_Version dceVersion);
};

#endif /* kern_support_hpp */
