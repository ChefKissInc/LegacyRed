//  Copyright © 2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

//  Copyright © 2023 Zormeister. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_lred.hpp"
#include "kern_gfx7con.hpp"
#include "kern_gfx8con.hpp"
#include "kern_model.hpp"
#include "kern_patches.hpp"
#include "kern_support.hpp"
#include "kern_x4000.hpp"
#include "kern_x4050_hwlibs.hpp"
#include "kern_x4070_hwlibs.hpp"
#include <Headers/kern_api.hpp>
#include <Headers/kern_devinfo.hpp>
#include <IOKit/IODeviceTreeSupport.h>

static const char *pathAGDP = "/System/Library/Extensions/AppleGraphicsControl.kext/Contents/PlugIns/"
                              "AppleGraphicsDevicePolicy.kext/Contents/MacOS/AppleGraphicsDevicePolicy";
static const char *pathBacklight = "/System/Library/Extensions/AppleBacklight.kext/Contents/MacOS/AppleBacklight";
static const char *pathMCCSControl = "/System/Library/Extensions/AppleMCCSControl.kext/Contents/MacOS/AppleMCCSControl";

static KernelPatcher::KextInfo kextAGDP {"com.apple.driver.AppleGraphicsDevicePolicy", &pathAGDP, 1, {true}, {},
    KernelPatcher::KextInfo::Unloaded};
static KernelPatcher::KextInfo kextBacklight {"com.apple.driver.AppleBacklight", &pathBacklight, 1, {true}, {},
    KernelPatcher::KextInfo::Unloaded};
static KernelPatcher::KextInfo kextMCCSControl {"com.apple.driver.AppleMCCSControl", &pathMCCSControl, 1, {true}, {},
    KernelPatcher::KextInfo::Unloaded};

LRed *LRed::callback = nullptr;

static GFX7Con gfx7con;
static GFX8Con gfx8con;
static Support support;
static X4050HWLibs x4050hwlibs;
static X4070HWLibs x4070hwlibs;
static X4000 x4000;

void LRed::init() {
    SYSLOG("lred", "Please don't support tonymacx86.com!");
    callback = this;

    lilu.onPatcherLoadForce(
        [](void *user, KernelPatcher &patcher) { static_cast<LRed *>(user)->processPatcher(patcher); }, this);
    lilu.onKextLoadForce(
        nullptr, 0,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            static_cast<LRed *>(user)->processKext(patcher, index, address, size);
        },
        this);
    lilu.onKextLoadForce(&kextAGDP);
    lilu.onKextLoadForce(&kextBacklight);
    lilu.onKextLoadForce(&kextMCCSControl);
    x4050hwlibs.init();
    x4070hwlibs.init();
    gfx7con.init();
    gfx8con.init();
    x4000.init();
    support.init();
}

void LRed::processPatcher(KernelPatcher &patcher) {
    auto *devInfo = DeviceInfo::create();
    if (devInfo) {
        devInfo->processSwitchOff();

        PANIC_COND(!devInfo->videoBuiltin, "lred", "videoBuiltin null");
        this->iGPU = OSDynamicCast(IOPCIDevice, devInfo->videoBuiltin);
        PANIC_COND(!this->iGPU, "lred", "videoBuiltin is not IOPCIDevice");
        PANIC_COND(WIOKit::readPCIConfigValue(iGPU, WIOKit::kIOPCIConfigVendorID) != WIOKit::VendorID::ATIAMD, "lred",
            "videoBuiltin is not AMD");

        WIOKit::renameDevice(this->iGPU, "IGPU");
        WIOKit::awaitPublishing(this->iGPU);
        char name[256] = {0};
        for (size_t i = 0, ii = 0; i < devInfo->videoExternal.size(); i++) {
            auto *device = OSDynamicCast(IOPCIDevice, devInfo->videoExternal[i].video);
            if (device) {
                snprintf(name, arrsize(name), "GFX%zu", ii++);
                WIOKit::renameDevice(device, name);
                WIOKit::awaitPublishing(device);
            }
        }

        static uint8_t builtin[] = {0x01};
        this->iGPU->setProperty("built-in", builtin, arrsize(builtin));
        this->deviceId = WIOKit::readPCIConfigValue(this->iGPU, WIOKit::kIOPCIConfigDeviceID);
        auto *model = getBranding(this->deviceId, WIOKit::readPCIConfigValue(iGPU, WIOKit::kIOPCIConfigRevisionID));
        if (model) {
            auto len = static_cast<uint32_t>(strlen(model) + 1);
            this->iGPU->setProperty("model", const_cast<char *>(model), len);
            this->iGPU->setProperty("ATY,FamilyName", const_cast<char *>("Radeon"), 7);
            this->iGPU->setProperty("ATY,DeviceName", const_cast<char *>(model) + 11,
                len - 11); /** TODO: Figure out if this works on LRed or not */
        }

        if (UNLIKELY(this->iGPU->getProperty("ATY,bin_image"))) {
            DBGLOG("lred", "VBIOS manually overridden");
        } else {
            if (!this->getVBIOSFromVFCT(this->iGPU)) {
                SYSLOG("lred", "Failed to get VBIOS from VFCT.");
                PANIC_COND(!this->getVBIOSFromVRAM(this->iGPU), "lred", "Failed to get VBIOS from VRAM");
            }
        }

        DeviceInfo::deleter(devInfo);
    } else {
        SYSLOG("lred", "Failed to create DeviceInfo");
    }

    KernelPatcher::RouteRequest requests[] = {
        {"__ZN15OSMetaClassBase12safeMetaCastEPKS_PK11OSMetaClass", wrapSafeMetaCast, this->orgSafeMetaCast},
        {"_cs_validate_page", csValidatePage, this->orgCsValidatePage},
    };

    size_t num = arrsize(requests);
    if (lilu.getRunMode() & LiluAPI::RunningNormal) {
        auto *entry = IORegistryEntry::fromPath("/", gIODTPlane);
        if (entry) {
            DBGLOG("lred", "Setting hwgva-id to iMacPro1,1");
            entry->setProperty("hwgva-id", const_cast<char *>("Mac-7BA5B2D9E42DDD94"),
                static_cast<uint32_t>(sizeof("Mac-7BA5B2D9E42DDD94")));
            entry->release();
        }
    } else {
        num -= 1;
    }
    PANIC_COND(!patcher.routeMultipleLong(KernelPatcher::KernelID, requests, num), "lred",
        "Failed to route kernel symbols");
}

OSMetaClassBase *LRed::wrapSafeMetaCast(const OSMetaClassBase *anObject, const OSMetaClass *toMeta) {
    auto ret = FunctionCast(wrapSafeMetaCast, callback->orgSafeMetaCast)(anObject, toMeta);
    if (!ret) {
        for (const auto &ent : callback->metaClassMap) {
            if (ent[0] == toMeta) {
                return FunctionCast(wrapSafeMetaCast, callback->orgSafeMetaCast)(anObject, ent[1]);
            } else if (UNLIKELY(ent[1] == toMeta)) {
                return FunctionCast(wrapSafeMetaCast, callback->orgSafeMetaCast)(anObject, ent[0]);
            }
        }
    }
    return ret;
}

void LRed::csValidatePage(vnode *vp, memory_object_t pager, memory_object_offset_t page_offset, const void *data,
    int *validated_p, int *tainted_p, int *nx_p) {
    FunctionCast(csValidatePage, callback->orgCsValidatePage)(vp, pager, page_offset, data, validated_p, tainted_p,
        nx_p);

    char path[PATH_MAX];
    int pathlen = PATH_MAX;
    if (!vn_getpath(vp, path, &pathlen)) {
        if (UserPatcher::matchSharedCachePath(path)) {
            if (UNLIKELY(
                    KernelPatcher::findAndReplace(const_cast<void *>(data), PAGE_SIZE, kVideoToolboxDRMModelOriginal,
                        arrsize(kVideoToolboxDRMModelOriginal), BaseDeviceInfo::get().modelIdentifier, 20)))
                DBGLOG("lred", "Relaxed VideoToolbox DRM model check");

            if (UNLIKELY(KernelPatcher::findAndReplace(const_cast<void *>(data), PAGE_SIZE, kBoardIdOriginal,
                    arrsize(kBoardIdOriginal), kBoardIdPatched, arrsize(kBoardIdPatched))))
                DBGLOG("lred", "Patched 'board-id' -> 'hwgva-id'");
        } else if (UNLIKELY(!strncmp(path, kCoreLSKDMSEPath, arrsize(kCoreLSKDMSEPath))) ||
                   UNLIKELY(!strncmp(path, kCoreLSKDPath, arrsize(kCoreLSKDPath)))) {
            if (UNLIKELY(KernelPatcher::findAndReplace(const_cast<void *>(data), PAGE_SIZE, kCoreLSKDOriginal,
                    arrsize(kCoreLSKDOriginal), kCoreLSKDPatched, arrsize(kCoreLSKDPatched))))
                DBGLOG("lred", "Patched streaming CPUID to Haswell");
        }
    }
}

void LRed::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (UNLIKELY(!this->rmmio || !this->rmmio->getLength())) {
        this->rmmio = this->iGPU->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress5);
        PANIC_COND(!this->rmmio || !this->rmmio->getLength(), "lred", "Failed to map RMMIO");
        this->rmmioPtr = reinterpret_cast<uint32_t *>(this->rmmio->getVirtualAddress());

        this->fbOffset = static_cast<uint64_t>(this->readReg32(0x296B)) << 24;
        this->revision = (this->readReg32(0xD2F) & 0xF000000) >> 0x18;
        switch (this->deviceId) {
            // Who thought it would be a good idea to use this many Device IDs and Revisions?
            case 0x1309:
                [[fallthrough]];
            case 0x130A:
                [[fallthrough]];
            case 0x130B:
                [[fallthrough]];
            case 0x130C:
                [[fallthrough]];
            case 0x130D:
                [[fallthrough]];
            case 0x130E:
                [[fallthrough]];
            case 0x130F:
                [[fallthrough]];
            case 0x1313:
                [[fallthrough]];
            case 0x1315:
                this->chipType = ChipType::Kaveri;
                this->gfxVer = GFXVersion::GFX7;
                DBGLOG("", "Chip type is Kaveri");
                break;
            case 0x1316:
                this->chipType = ChipType::Kaveri;
                this->gfxVer = GFXVersion::GFX7;
                DBGLOG("lred", "Chip type is Kaveri");
                this->chipVariant = ChipVariant::Spooky;
                /** Unusure about this one, found through GPUOpen's DeviceInfo, will research */
                DBGLOG("lred", "Chip variant is Spooky");
            case 0x1318:
                [[fallthrough]];
            case 0x131B:
                [[fallthrough]];
            case 0x131C:
                [[fallthrough]];
            case 0x131D:
                this->chipType = ChipType::Kaveri;
                this->gfxVer = GFXVersion::GFX7;
                DBGLOG("lred", "Chip type is Kaveri");
                break;
            case 0x9830:
                [[fallthrough]];
            case 0x9831:
                [[fallthrough]];
            case 0x9832:
                [[fallthrough]];
            case 0x9833:
                [[fallthrough]];
            case 0x9834:
                [[fallthrough]];
            case 0x9835:
                [[fallthrough]];
            case 0x9836:
                [[fallthrough]];
            case 0x9837:
                [[fallthrough]];
            case 0x9838:
                [[fallthrough]];
            case 0x9839:
                [[fallthrough]];
            case 0x983D:
                this->chipType = ChipType::Kabini;
                this->gfxVer = GFXVersion::GFX7;
                DBGLOG("lred", "Chip type is Kabini");
                if (this->deviceId == 0x9831 || this->deviceId == 0x9833 || this->deviceId == 0x9835 ||
                    this->deviceId == 0x9837) {
                    DBGLOG("lred", "Chip variant is Kabini 'E'");
                    this->chipVariant = ChipVariant::KLE;
                }
                break;
            case 0x9850:
                [[fallthrough]];
            case 0x9851:
                [[fallthrough]];
            case 0x9852:
                [[fallthrough]];
            case 0x9853:
                [[fallthrough]];
            case 0x9854:
                [[fallthrough]];
            case 0x9855:
                [[fallthrough]];
            case 0x9856:
                this->chipType = ChipType::Mullins;
                this->gfxVer = GFXVersion::GFX7;
                DBGLOG("lred", "Chip type is Mullins");
                switch (this->deviceId) {
                    case 0x9851:
                        if (this->revision == 0x01 || this->revision == 0x06) {
                            this->chipVariant = ChipVariant::KLE;
                            DBGLOG("lred", "Chip variant is Mullins 'E'");
                        }
                        break;
                    case 0x9853:
                        if (this->revision == 0x01 || (this->revision >= 0x05 && this->revision < 0x40)) {
                            this->chipVariant = ChipVariant::KLE;
                            DBGLOG("lred", "Chip variant is Mullins 'E'");
                        }
                        break;
                    case 0x9854:
                        if (this->revision == 0x01) {
                            this->chipVariant = ChipVariant::KLE;
                            DBGLOG("lred", "Chip variant is Mullins 'E'");
                        }
                        break;
                    case 0x9856:
                        if (this->revision >= 0x01 && this->revision != 0x02 && this->revision != 0x06) {
                            this->chipVariant = ChipVariant::KLE;
                            DBGLOG("lred", "Chip variant is Mullins 'E'");
                        }
                    default:
                        break;
                }
                break;
            case 0x9874:
                this->chipType = ChipType::Carrizo;
                DBGLOG("lred", "Chip type is Carrizo");
                this->gfxVer = GFXVersion::GFX8;
                if (this->revision >= 0xC8) {
                    this->chipVariant = ChipVariant::Bristol;
                    DBGLOG("lred", "Chip variant is Bristol");
                }
                break;
            case 0x98E4:
                this->chipType = ChipType::Stoney;
                this->gfxVer = GFXVersion::GFX8;
                DBGLOG("lred", "Chip type is Stoney");
                /** R4 and up iGPUs have 3 compute units while the others have 2 CUs, hence the chip variations */
                if (this->revision <= 0x81 || (this->revision >= 0xC0 && this->revision < 0xD0) ||
                    (this->revision >= 0xD9 && this->revision < 0xDB) || this->revision >= 0xE9) {
                    this->chipVariant = ChipVariant::s3CU;
                    DBGLOG("lred", "Chip variant is Stoney 3CU");
                } else {
                    this->chipVariant = ChipVariant::s2CU;
                    DBGLOG("lred", "Chip variant is Stoney 2CU");
                }
            default:
                PANIC("lred", "Unknown device ID");
        }
    }
    if (this->gfxVer == GFXVersion::GFX7) {
        DBGLOG("lred", "GFX version is 7");
    } else {
        DBGLOG("lred", "GFX version is 8");
    }
    if (kextBacklight.loadIndex == index) {
        KernelPatcher::RouteRequest request {"__ZN15AppleIntelPanel10setDisplayEP9IODisplay", wrapApplePanelSetDisplay,
            orgApplePanelSetDisplay};
        if (patcher.routeMultiple(kextBacklight.loadIndex, &request, 1, address, size)) {
            const uint8_t find[] = {"F%uT%04x"};
            const uint8_t replace[] = {"F%uTxxxx"};
            KernelPatcher::LookupPatch patch = {&kextBacklight, find, replace, sizeof(find), 1};
            DBGLOG("lred", "applying backlight patch");
            patcher.applyLookupPatch(&patch);
        }
    } else if (kextMCCSControl.loadIndex == index) {
        KernelPatcher::RouteRequest request[] = {
            {"__ZN25AppleMCCSControlGibraltar5probeEP9IOServicePi", wrapFunctionReturnZero},
            {"__ZN21AppleMCCSControlCello5probeEP9IOServicePi", wrapFunctionReturnZero},
        };
        patcher.routeMultiple(index, request, address, size);
    } else if (support.processKext(patcher, index, address, size)) {
        DBGLOG("lred", "Processed support");
    } else if (x4050hwlibs.processKext(patcher, index, address, size)) {
        DBGLOG("lred", "Processed x4050hwlibs");
    } else if (x4070hwlibs.processKext(patcher, index, address, size)) {
        DBGLOG("lred", "Processed x4070hwlibs");
    } else if (gfx7con.processKext(patcher, index, address, size)) {
        DBGLOG("lred", "Processed gfx7con");
    } else if (gfx8con.processKext(patcher, index, address, size)) {
        DBGLOG("lred", "Processed gfx8con");
    } else if (x4000.processKext(patcher, index, address, size)) {
        DBGLOG("lred", "Processed x4000");
    }
}

struct ApplePanelData {
    const char *deviceName;
    uint8_t deviceData[36];
};

static ApplePanelData appleBacklightData[] = {
    {"F14Txxxx", {0x00, 0x11, 0x00, 0x00, 0x00, 0x34, 0x00, 0x52, 0x00, 0x73, 0x00, 0x94, 0x00, 0xBE, 0x00, 0xFA, 0x01,
                     0x36, 0x01, 0x72, 0x01, 0xC5, 0x02, 0x2F, 0x02, 0xB9, 0x03, 0x60, 0x04, 0x1A, 0x05, 0x0A, 0x06,
                     0x0E, 0x07, 0x10}},
    {"F15Txxxx", {0x00, 0x11, 0x00, 0x00, 0x00, 0x36, 0x00, 0x54, 0x00, 0x7D, 0x00, 0xB2, 0x00, 0xF5, 0x01, 0x49, 0x01,
                     0xB1, 0x02, 0x2B, 0x02, 0xB8, 0x03, 0x59, 0x04, 0x13, 0x04, 0xEC, 0x05, 0xF3, 0x07, 0x34, 0x08,
                     0xAF, 0x0A, 0xD9}},
    {"F16Txxxx", {0x00, 0x11, 0x00, 0x00, 0x00, 0x18, 0x00, 0x27, 0x00, 0x3A, 0x00, 0x52, 0x00, 0x71, 0x00, 0x96, 0x00,
                     0xC4, 0x00, 0xFC, 0x01, 0x40, 0x01, 0x93, 0x01, 0xF6, 0x02, 0x6E, 0x02, 0xFE, 0x03, 0xAA, 0x04,
                     0x78, 0x05, 0x6C}},
    {"F17Txxxx", {0x00, 0x11, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x34, 0x00, 0x4F, 0x00, 0x71, 0x00, 0x9B, 0x00, 0xCF, 0x01,
                     0x0E, 0x01, 0x5D, 0x01, 0xBB, 0x02, 0x2F, 0x02, 0xB9, 0x03, 0x60, 0x04, 0x29, 0x05, 0x1E, 0x06,
                     0x44, 0x07, 0xA1}},
    {"F18Txxxx", {0x00, 0x11, 0x00, 0x00, 0x00, 0x53, 0x00, 0x8C, 0x00, 0xD5, 0x01, 0x31, 0x01, 0xA2, 0x02, 0x2E, 0x02,
                     0xD8, 0x03, 0xAE, 0x04, 0xAC, 0x05, 0xE5, 0x07, 0x59, 0x09, 0x1C, 0x0B, 0x3B, 0x0D, 0xD0, 0x10,
                     0xEA, 0x14, 0x99}},
    {"F19Txxxx", {0x00, 0x11, 0x00, 0x00, 0x02, 0x8F, 0x03, 0x53, 0x04, 0x5A, 0x05, 0xA1, 0x07, 0xAE, 0x0A, 0x3D, 0x0E,
                     0x14, 0x13, 0x74, 0x1A, 0x5E, 0x24, 0x18, 0x31, 0xA9, 0x44, 0x59, 0x5E, 0x76, 0x83, 0x11, 0xB6,
                     0xC7, 0xFF, 0x7B}},
    {"F24Txxxx", {0x00, 0x11, 0x00, 0x01, 0x00, 0x34, 0x00, 0x52, 0x00, 0x73, 0x00, 0x94, 0x00, 0xBE, 0x00, 0xFA, 0x01,
                     0x36, 0x01, 0x72, 0x01, 0xC5, 0x02, 0x2F, 0x02, 0xB9, 0x03, 0x60, 0x04, 0x1A, 0x05, 0x0A, 0x06,
                     0x0E, 0x07, 0x10}},
};

size_t LRed::wrapFunctionReturnZero() { return 0; }

bool LRed::wrapApplePanelSetDisplay(IOService *that, IODisplay *display) {
    static bool once = false;
    if (!once) {
        once = true;
        auto panels = OSDynamicCast(OSDictionary, that->getProperty("ApplePanels"));
        if (panels) {
            auto rawPanels = panels->copyCollection();
            panels = OSDynamicCast(OSDictionary, rawPanels);

            if (panels) {
                for (auto &entry : appleBacklightData) {
                    auto pd = OSData::withBytes(entry.deviceData, sizeof(entry.deviceData));
                    if (pd) {
                        panels->setObject(entry.deviceName, pd);
                        // No release required by current AppleBacklight implementation.
                    } else {
                        SYSLOG("lred", "Panel start cannot allocate %s data", entry.deviceName);
                    }
                }
                that->setProperty("ApplePanels", panels);
            }

            if (rawPanels) { rawPanels->release(); }
        } else {
            SYSLOG("lred", "Panel start has no panels");
        }
    }

    bool result = FunctionCast(wrapApplePanelSetDisplay, callback->orgApplePanelSetDisplay)(that, display);
    DBGLOG("lred", "Panel display set returned %d", result);

    return result;
}
