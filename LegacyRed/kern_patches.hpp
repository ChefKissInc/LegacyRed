//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_patches_hpp
#define kern_patches_hpp
#include <Headers/kern_util.hpp>

/**
 * `AtiDeviceControl::getVendorInfo`
 * `AMDSupport.kext`
 *  Tells AGDC we're an iGPU
 */
static const uint8_t kAtiDeviceControlGetVendorInfoOriginal[] = {0xC7, 0x00, 0x24, 0x02, 0x10, 0x00, 0x00, 0x48, 0x00,
    0x4D, 0xE8, 0xC7, 0x00, 0x28, 0x02, 0x00, 0x00, 0x00};
static const uint8_t kAtiDeviceControlGetVendorInfoMask[] = {0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF,
    0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t kAtiDeviceControlGetVendorInfoPatched[] = {0xC7, 0x00, 0x24, 0x02, 0x10, 0x00, 0x00, 0x48, 0x00,
    0x4D, 0xE8, 0xC7, 0x00, 0x28, 0x01, 0x00, 0x00, 0x00};

/**
 * `ASIC_INFO__::populateDeviceInfo`
 * `AMD8000Controller.kext, AMD9000Controller.kext & AMD9500Controller.kext`
 * familyId to 0x7D for GCN 2 and 0x87 for GCN 3
 */
static const uint8_t kAsicInfoCIPopulateDeviceInfoOriginal[] = {0x48, 0x8B, 0x4D, 0xE8, 0xC7, 0x41, 0x40, 0x78, 0x00,
    0x00, 0x00, 0x8B, 0x45, 0xF0};
static const uint8_t kAsicInfoCIPopulateDeviceInfoPatched[] = {0x48, 0x8B, 0x4D, 0xE8, 0xC7, 0x41, 0x40, 0x7D, 0x00,
    0x00, 0x00, 0x8B, 0x45, 0xF0};
// applies to AMD9000 & AMD9500.
static const uint8_t kAsicInfoVIPopulateDeviceInfoOriginal[] = {0x48, 0x8B, 0x4D, 0xE8, 0xC7, 0x41, 0x40, 0x82, 0x00,
    0x00, 0x00, 0x8B, 0x45, 0xF0};
static const uint8_t kAsicInfoVIPopulateDeviceInfoPatched[] = {0x48, 0x8B, 0x4D, 0xE8, 0xC7, 0x41, 0x40, 0x87, 0x00,
    0x00, 0x00, 0x8B, 0x45, 0xF0};

/**
 * `SharedController::getFamilyId`
 * `AMD8000Controller.kext, AMD9000Controller.kext, AMD9500Controller.kext`
 * Force familyId to 0x7D or 0x87
 */
static const uint8_t kCISharedControllerGetFamilyIdOriginal[] = {0x66, 0xC7, 0x45, 0xF6, 0x00, 0x00, 0x66, 0xC7, 0x45,
    0xF6, 0x78, 0x00, 0x0F, 0xB7, 0x45, 0xF6};
static const uint8_t kCISharedControllerGetFamilyIdPatched[] = {0x66, 0xC7, 0x45, 0xF6, 0x00, 0x00, 0x66, 0xC7, 0x45,
    0xF6, 0x7D, 0x00, 0x0F, 0xB7, 0x45, 0xF6};

static const uint8_t kVIBaffinSharedControllerGetFamilyIdOriginal[] = {0x66, 0xC7, 0x45, 0xF6, 0x00, 0x00, 0x66, 0xC7,
    0x45, 0xF6, 0x82, 0x00, 0x0F, 0xB7, 0x45, 0xF6};
static const uint8_t kVIBaffinSharedControllerGetFamilyIdPatched[] = {0x66, 0xC7, 0x45, 0xF6, 0x00, 0x00, 0x66, 0xC7,
    0x45, 0xF6, 0x87, 0x00, 0x0F, 0xB7, 0x45, 0xF6};

/**
 * `AtiBiosParser1::setDisplayClock`
 * `AMDSupport.kext`
 *  Bypass m_setDceClock Null check
 */
static const uint8_t kAtiBiosParser1SetDisplayClockOriginal[] = {0x48, 0x85, 0xFF, 0x74, 0x29};
static const uint8_t kAtiBiosParser1SetDisplayClockPatched[] = {0x48, 0x85, 0xFF, 0x66, 0x90};

/**
 * `AppleGraphicsDevicePolicy`
 * Symbols are stripped so function is unknown.
 * Removes framebuffer count >= 2 check.
 */
static const uint8_t kAGDPFBCountCheckOriginal[] = {0x02, 0x00, 0x00, 0x83, 0xF8, 0x02};
static const uint8_t kAGDPFBCountCheckPatched[] = {0x02, 0x00, 0x00, 0x83, 0xF8, 0x00};

/**
 * `AppleGraphicsDevicePolicy::start`
 * Neutralise access to AGDP configuration by board identifier.
 */
static const char kAGDPBoardIDKeyOriginal[] = "board-id";
static const char kAGDPBoardIDKeyPatched[] = "applehax";

/**
 * `AMDRadeonX4000_AMDHardware::startHWEngines`
 * Make for loop run only once as we only have one SDMA engine.
 * Patch originally came from NootedRed, since the code for startHWEngines is nearly identical on X4000, this patch, in
 * theory, should work
 */
static const uint8_t kStartHWEnginesOriginal[] = {0x40, 0x83, 0xF0, 0x02};
static const uint8_t kStartHWEnginesMask[] = {0xF0, 0xFF, 0xF0, 0xFF};
static const uint8_t kStartHWEnginesPatched[] = {0x40, 0x83, 0xF0, 0x01};

/**
* `AMDRadeonX4000_AMDEllesmereHardware::allocateHWEngines`
* Erase the 2nd SDMA engine
* This works for us, since we don't mess with the HWEngines
*/
static const char kAMDEllesmereHWallocHWEnginesOriginal[] = {0xE8, 0xAA, 0xF3, 0xFE, 0xFF, 0x48, 0x89, 0xC3, 0x48, 0x89, 0xC7, 0xE8, 0xA9, 0xF3, 0xFE, 0xFF, 0x49, 0x89, 0x9E, 0xC0, 0x03, 0x00, 0x00};
static const char kAMDEllesmereHWallocHWEnginesPatched[] = {0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x66, 0x90, 0x90};

/** VideoToolbox DRM model check */
static const char kVideoToolboxDRMModelOriginal[] = "MacPro5,1\0MacPro6,1\0IOService";

static const uint8_t kBoardIdOriginal[] = {0x62, 0x6F, 0x61, 0x72, 0x64, 0x2D, 0x69, 0x64, 0x00, 0x68, 0x77, 0x2E, 0x6D,
    0x6F, 0x64, 0x65, 0x6C};
static const uint8_t kBoardIdPatched[] = {0x68, 0x77, 0x67, 0x76, 0x61};

static const char kCoreLSKDMSEPath[] = "/System/Library/PrivateFrameworks/CoreLSKDMSE.framework/Versions/A/CoreLSKDMSE";
static const char kCoreLSKDPath[] = "/System/Library/PrivateFrameworks/CoreLSKD.framework/Versions/A/CoreLSKD";

static const uint8_t kCoreLSKDOriginal[] = {0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, 0x0F, 0xA2};
static const uint8_t kCoreLSKDPatched[] = {0xC7, 0xC0, 0xC3, 0x06, 0x03, 0x00, 0x90, 0x90};

static_assert(arrsize(kAtiDeviceControlGetVendorInfoOriginal) == arrsize(kAtiDeviceControlGetVendorInfoPatched));
static_assert(arrsize(kAsicInfoCIPopulateDeviceInfoOriginal) == arrsize(kAsicInfoCIPopulateDeviceInfoPatched));
static_assert(arrsize(kAsicInfoVIPopulateDeviceInfoOriginal) == arrsize(kAsicInfoVIPopulateDeviceInfoPatched));
static_assert(arrsize(kCISharedControllerGetFamilyIdOriginal) == arrsize(kCISharedControllerGetFamilyIdPatched));
static_assert(
    arrsize(kVIBaffinSharedControllerGetFamilyIdOriginal) == arrsize(kVIBaffinSharedControllerGetFamilyIdPatched));
static_assert(arrsize(kAtiBiosParser1SetDisplayClockOriginal) == arrsize(kAtiBiosParser1SetDisplayClockPatched));
static_assert(arrsize(kAGDPFBCountCheckOriginal) == arrsize(kAGDPFBCountCheckPatched));
static_assert(arrsize(kAGDPBoardIDKeyOriginal) == arrsize(kAGDPBoardIDKeyPatched));
static_assert(arrsize(kStartHWEnginesOriginal) == arrsize(kStartHWEnginesPatched));
static_assert(arrsize(kCoreLSKDOriginal) == arrsize(kCoreLSKDPatched));

#endif /* kern_patches_hpp */
