//  Copyright © 2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "LRed.hpp"
#include "GFXCon.hpp"
#include "HWLibs.hpp"
#include "Model.hpp"
#include "Support.hpp"
#include "X4000.hpp"
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

static GFXCon gfxcon;
static Support support;
static HWLibs hwlibs;
static X4000 x4000;

void LRed::init() {
    SYSLOG("LRed", "Copyright © 2023 ChefKiss Inc. If you've paid for this, you've been scammed.");
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
    hwlibs.init();
    gfxcon.init();
    x4000.init();
    support.init();
}

void LRed::processPatcher(KernelPatcher &patcher) {
    auto *devInfo = DeviceInfo::create();
    if (devInfo) {
        devInfo->processSwitchOff();

        if (!devInfo->videoBuiltin) {
            for (size_t i = 0; i < devInfo->videoExternal.size(); i++) {
                auto *device = OSDynamicCast(IOPCIDevice, devInfo->videoExternal[i].video);
                if (!device) { continue; }
                auto devid = WIOKit::readPCIConfigValue(device, WIOKit::kIOPCIConfigDeviceID) & 0xFF00;
                if (WIOKit::readPCIConfigValue(device, WIOKit::kIOPCIConfigVendorID) == WIOKit::VendorID::ATIAMD &&
                    (devid == 0x1300 || devid == 0x9800)) {
                    this->iGPU = device;
                    break;
                }
            }
            PANIC_COND(!this->iGPU, "LRed", "No iGPU found");
        } else {
            PANIC_COND(!devInfo->videoBuiltin, "LRed", "videoBuiltin null");
            this->iGPU = OSDynamicCast(IOPCIDevice, devInfo->videoBuiltin);
            PANIC_COND(!this->iGPU, "LRed", "videoBuiltin is not IOPCIDevice");
            PANIC_COND(WIOKit::readPCIConfigValue(this->iGPU, WIOKit::kIOPCIConfigVendorID) != WIOKit::VendorID::ATIAMD,
                "LRed", "videoBuiltin is not AMD");
        }

        WIOKit::renameDevice(this->iGPU, "IGPU");
        WIOKit::awaitPublishing(this->iGPU);

        static uint8_t builtin[] = {0x00};
        this->iGPU->setProperty("built-in", builtin, arrsize(builtin));
        this->deviceId = WIOKit::readPCIConfigValue(this->iGPU, WIOKit::kIOPCIConfigDeviceID);
        this->pciRevision = WIOKit::readPCIConfigValue(LRed::callback->iGPU, WIOKit::kIOPCIConfigRevisionID);

        this->iGPU->setMemoryEnable(true);

        char name[128] = {0};
        for (size_t i = 0, ii = 0; i < devInfo->videoExternal.size(); i++) {
            auto *device = OSDynamicCast(IOPCIDevice, devInfo->videoExternal[i].video);
            if (device && WIOKit::readPCIConfigValue(device, WIOKit::kIOPCIConfigDeviceID) != this->deviceId) {
                snprintf(name, arrsize(name), "GFX%zu", ii++);
                WIOKit::renameDevice(device, name);
                WIOKit::awaitPublishing(device);
            }
        }

        if (UNLIKELY(this->iGPU->getProperty("ATY,bin_image"))) {
            DBGLOG("LRed", "VBIOS manually overridden");
        } else {
            if (!this->getVBIOSFromVFCT(this->iGPU)) {
                SYSLOG("LRed", "Failed to get VBIOS from VFCT.");
                PANIC_COND(!this->getVBIOSFromVRAM(this->iGPU), "LRed", "Failed to get VBIOS from VRAM");
            }
        }

        DeviceInfo::deleter(devInfo);
    } else {
        SYSLOG("LRed", "Failed to create DeviceInfo");
    }

    KernelPatcher::RouteRequest requests[] = {
        {"__ZN15OSMetaClassBase12safeMetaCastEPKS_PK11OSMetaClass", wrapSafeMetaCast, this->orgSafeMetaCast},
    };

    size_t num = arrsize(requests);
    PANIC_COND(!patcher.routeMultipleLong(KernelPatcher::KernelID, requests, num), "LRed",
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

void LRed::setRMMIOIfNecessary() {
    if (UNLIKELY(!this->rmmio || !this->rmmio->getLength())) {
        this->rmmio = this->iGPU->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress5);
        PANIC_COND(!this->rmmio || !this->rmmio->getLength(), "LRed", "Failed to map RMMIO");
        this->rmmioPtr = reinterpret_cast<uint32_t *>(this->rmmio->getVirtualAddress());
        this->fbOffset = static_cast<uint64_t>(this->readReg32(0x081A)) << 22;
        SYSLOG("LRed", "Gathered framebuffer offset: 0x%llX", this->fbOffset);
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
                [[fallthrough]];
            case 0x1316:
                [[fallthrough]];
            case 0x1318:
                [[fallthrough]];
            case 0x131B:
                [[fallthrough]];
            case 0x131C:
                [[fallthrough]];
            case 0x131D:
                this->currentFamilyId = AMDGPU_FAMILY_KV;
                if (this->deviceId == 0x1312 || this->deviceId == 0x1316 || this->deviceId == 0x1317) {
                    this->chipType = ChipType::Spooky;
                    this->chipVariant = ChipVariant::Kaveri;
                    DBGLOG("LRed", "Chip type is Spooky, Chip variant is Kaveri");
                    this->enumeratedRevision = 0x41;
                    break;
                }
                this->chipType = ChipType::Spectre;
                this->chipVariant = ChipVariant::Kaveri;
                DBGLOG("LRed", "Chip type is Spectre, Chip variant is Kaveri");
                this->enumeratedRevision = 0x1;
                this->revision = (this->readReg32(0x1559) & 0xF000000) >> 0x1c;
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
                this->currentFamilyId = AMDGPU_FAMILY_KV;
                if (this->revision == 0x00) {
                    this->chipType = ChipType::Kalindi;
                    this->chipVariant = ChipVariant::Kabini;
                    DBGLOG("LRed", "Chip type is Kalindi, Chip variant is Kabini");
                    this->enumeratedRevision = 0x81;
                    break;
                } else if (this->revision == 0x01) {
                    this->chipType = ChipType::Kalindi;
                    this->chipVariant = ChipVariant::Kabini;
                    DBGLOG("LRed", "Chip type is Kalindi, Chip variant is Kabini");
                    this->enumeratedRevision = 0x82;
                    break;
                } else if (this->revision == 0x02) {
                    this->chipType = ChipType::Kalindi;
                    this->chipVariant = ChipVariant::Bhavani;
                    DBGLOG("LRed", "Chip type is Kabini, Chip variant is Bhavani; using A1 DDI Caps for HWLibs");
                    this->enumeratedRevision = 0x85;
                    break;
                }
                this->revision = (this->readReg32(0x1559) & 0xF000000) >> 0x1c;
                break;
            case 0x9850:
                this->chipType = ChipType::Godavari;
                this->chipVariant = ChipVariant::Mullins;
                this->enumeratedRevision = 0xA1;
                this->currentFamilyId = AMDGPU_FAMILY_KV;
                DBGLOG("LRed", "Chip type is Godavari, Chip variant is Mullins");
                this->revision = (this->readReg32(0x1559) & 0xF000000) >> 0x1c;
                break;
            case 0x9851:
                this->chipType = ChipType::Godavari;
                this->chipVariant = ChipVariant::Mullins;
                this->enumeratedRevision = 0xA1;
                this->currentFamilyId = AMDGPU_FAMILY_KV;
                DBGLOG("LRed", "Chip type is Godavari, Chip variant is Mullins");
                this->revision = (this->readReg32(0x1559) & 0xF000000) >> 0x1c;
                break;
            case 0x9852:
                this->chipType = ChipType::Godavari;
                this->chipVariant = ChipVariant::Mullins;
                this->enumeratedRevision = 0xA1;
                this->currentFamilyId = AMDGPU_FAMILY_KV;
                DBGLOG("LRed", "Chip type is Godavari, Chip variant is Mullins");
                this->revision = (this->readReg32(0x1559) & 0xF000000) >> 0x1c;
                break;
            case 0x9853:
                this->chipType = ChipType::Godavari;
                this->chipVariant = ChipVariant::Mullins;
                this->enumeratedRevision = 0xA1;
                this->currentFamilyId = AMDGPU_FAMILY_KV;
                DBGLOG("LRed", "Chip type is Godavari, Chip variant is Mullins");
                this->revision = (this->readReg32(0x1559) & 0xF000000) >> 0x1c;
                break;
            case 0x9854:
                this->chipType = ChipType::Godavari;
                this->chipVariant = ChipVariant::Mullins;
                this->enumeratedRevision = 0xA1;
                this->currentFamilyId = AMDGPU_FAMILY_KV;
                DBGLOG("LRed", "Chip type is Godavari, Chip variant is Mullins");
                this->revision = (this->readReg32(0x1559) & 0xF000000) >> 0x1c;
                break;
            case 0x9855:
                this->chipType = ChipType::Godavari;
                this->chipVariant = ChipVariant::Mullins;
                this->enumeratedRevision = 0xA1;
                this->currentFamilyId = AMDGPU_FAMILY_KV;
                DBGLOG("LRed", "Chip type is Godavari, Chip variant is Mullins");
                this->revision = (this->readReg32(0x1559) & 0xF000000) >> 0x1c;
                break;
            case 0x9856:
                this->chipType = ChipType::Godavari;
                this->chipVariant = ChipVariant::Mullins;
                this->enumeratedRevision = 0xA1;
                this->currentFamilyId = AMDGPU_FAMILY_KV;
                DBGLOG("LRed", "Chip type is Godavari, Chip variant is Mullins");
                this->revision = (this->readReg32(0x1559) & 0xF000000) >> 0x1c;
                break;
            case 0x9874:
                this->chipType = ChipType::Carrizo;
                DBGLOG("LRed", "Chip type is Carrizo");
                this->isGCN3 = true;
                this->enumeratedRevision = 0x1;
                this->revision = (smcReadReg32Cz(0xC0014044) & 0x00001E00) >> 9;
                this->currentFamilyId = AMDGPU_FAMILY_CZ;
                if ((this->revision >= 0xC8 && this->revision <= 0xCE) ||
                    (this->revision >= 0xE1 && this->revision <= 0xE6)) {
                    this->chipVariant = ChipVariant::Bristol;
                    DBGLOG("LRed", "Chip variant is Bristol");
                }
                break;
            case 0x98E4:
                this->chipType = ChipType::Stoney;
                this->isGCN3 = true;
                this->enumeratedRevision = 0x61;
                this->revision = (smcReadReg32Cz(0xC0014044) & 0x00001E00) >> 9;
                this->currentFamilyId = AMDGPU_FAMILY_CZ;
                DBGLOG("LRed", "Chip type is Stoney");
                /** R4 and up iGPUs have 3 compute units while the others have 2 CUs, hence the chip variations */
                if (this->revision <= 0x81 || (this->revision >= 0xC0 && this->revision < 0xD0) ||
                    (this->revision >= 0xD9 && this->revision < 0xDB) || this->revision >= 0xE9) {
                    this->isStoney3CU = true;
                    DBGLOG("LRed", "Chip has been identified as a 3CU model");
                    break;
                } else {
                    break;
                }
            default:
                PANIC("LRed", "Unknown device ID: %x", deviceId);
        }
        DBGLOG_COND(this->isGCN3, "LRed", "iGPU is GCN 3 derivative");
        this->currentEmulatedRevisionId =
            // why ChipType instead of ChipVariant? - For mullins we set it as 'Godavari', which is technically just
            // Kalindi+, by the looks of AMDGPU code
            ((LRed::callback->chipType == ChipType::Kalindi)) ?
                static_cast<uint32_t>(LRed::callback->enumeratedRevision) :
                static_cast<uint32_t>(LRed::callback->enumeratedRevision) + LRed::callback->revision;    // rough guess
    }
}

void LRed::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextBacklight.loadIndex == index) {
        KernelPatcher::RouteRequest request {"__ZN15AppleIntelPanel10setDisplayEP9IODisplay", wrapApplePanelSetDisplay,
            orgApplePanelSetDisplay};
        if (patcher.routeMultiple(kextBacklight.loadIndex, &request, 1, address, size)) {
            const uint8_t find[] = {"F%uT%04x"};
            const uint8_t replace[] = {"F%uTxxxx"};
            KernelPatcher::LookupPatch patch = {&kextBacklight, find, replace, sizeof(find), 1};
            DBGLOG("LRed", "applying backlight patch");
            patcher.applyLookupPatch(&patch);
        }
    } else if (kextMCCSControl.loadIndex == index) {
        KernelPatcher::RouteRequest request[] = {
            {"__ZN25AppleMCCSControlGibraltar5probeEP9IOServicePi", wrapFunctionReturnZero},
            {"__ZN21AppleMCCSControlCello5probeEP9IOServicePi", wrapFunctionReturnZero},
        };
        patcher.routeMultiple(index, request, address, size);
    } else if (support.processKext(patcher, index, address, size)) {
        DBGLOG("LRed", "Processed support");
    } else if (hwlibs.processKext(patcher, index, address, size)) {
        DBGLOG("LRed", "Processed hwlibs");
    } else if (gfxcon.processKext(patcher, index, address, size)) {
        DBGLOG("LRed", "Processed gfxcon");
    } else if (x4000.processKext(patcher, index, address, size)) {
        DBGLOG("LRed", "Processed x4000");
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
                        SYSLOG("LRed", "Panel start cannot allocate %s data", entry.deviceName);
                    }
                }
                that->setProperty("ApplePanels", panels);
            }

            if (rawPanels) { rawPanels->release(); }
        } else {
            SYSLOG("LRed", "Panel start has no panels");
        }
    }

    bool result = FunctionCast(wrapApplePanelSetDisplay, callback->orgApplePanelSetDisplay)(that, display);
    DBGLOG("LRed", "Panel display set returned %d", result);

    return result;
}
