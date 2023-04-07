//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_lred_hpp
#define kern_lred_hpp
#include "kern_amd.hpp"
#include "kern_fw.hpp"
#include "kern_vbios.hpp"
#include <Headers/kern_iokit.hpp>
#include <IOKit/acpi/IOACPIPlatformExpert.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/pci/IOPCIDevice.h>

class EXPORT PRODUCT_NAME : public IOService {
    OSDeclareDefaultStructors(PRODUCT_NAME);

    public:
    IOService *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
};

enum struct ChipType : uint32_t {
    Kaveri = 0,
    Kabini,
    Mullins,
    Carrizo,
    Stoney,
    Unknown,
};

enum struct ChipVariant : uint32_t {
    KVE = 0,
    Spooky,    // A revision of APU? More research needed
    KLE,       // Unsure about the three above
    s2CU,      // s2CU && s3CU have different Golden Settings
    s3CU,
    Bristol,    // Bristol is acctually just a Carrizo+, hence why it isn't in ChipType
    Normal,
};
// Primarily designed for X4000 where we have to use both GFX versions
enum struct GFXVersion {
    GFX7,
    GFX8,
    Unknown,
};
// Hack
class AppleACPIPlatformExpert : IOACPIPlatformExpert {
    friend class LRed;
};

// https://elixir.bootlin.com/linux/latest/source/drivers/gpu/drm/amd/amdgpu/amdgpu_bios.c#L49
static bool checkAtomBios(const uint8_t *bios, size_t size) {
    uint16_t tmp, bios_header_start;

    if (size < 0x49) {
        DBGLOG("lred", "VBIOS size is invalid");
        return false;
    }

    if (bios[0] != 0x55 || bios[1] != 0xAA) {
        DBGLOG("lred", "VBIOS signature <%x %x> is invalid", bios[0], bios[1]);
        return false;
    }

    bios_header_start = bios[0x48] | (bios[0x49] << 8);
    if (!bios_header_start) {
        DBGLOG("lred", "Unable to locate VBIOS header");
        return false;
    }

    tmp = bios_header_start + 4;
    if (size < tmp) {
        DBGLOG("lred", "BIOS header is broken");
        return false;
    }

    if (!memcmp(bios + tmp, "ATOM", 4) || !memcmp(bios + tmp, "MOTA", 4)) {
        DBGLOG("lred", "ATOMBIOS detected");
        return true;
    }

    return false;
}

class LRed {
    friend class GFX7Con;
    friend class GFX8Con;
    friend class X4050HWLibs;
    friend class X4070HWLibs;
    friend class X4000;
    friend class Support;

    public:
    static LRed *callback;

    void init();
    void processPatcher(KernelPatcher &patcher);
    void processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

    private:
    static const char *getChipName() {
        PANIC_COND(callback->chipType == ChipType::Unknown, "lred", "Unknown chip type");
        static const char *chipNames[] = {"kaveri", "kabini", "mullins", "carrizo", "stoney"};
        return chipNames[static_cast<int>(callback->chipType)];
    }

    bool getVBIOSFromVFCT(IOPCIDevice *obj) {
        DBGLOG("lred", "Fetching VBIOS from VFCT table");
        auto *expert = reinterpret_cast<AppleACPIPlatformExpert *>(obj->getPlatform());
        PANIC_COND(!expert, "lred", "Failed to get AppleACPIPlatformExpert");

        auto *vfctData = expert->getACPITableData("VFCT", 0);
        if (!vfctData) {
            DBGLOG("lred", "No VFCT from AppleACPIPlatformExpert");
            return false;
        }

        auto *vfct = static_cast<const VFCT *>(vfctData->getBytesNoCopy());
        PANIC_COND(!vfct, "lred", "VFCT OSData::getBytesNoCopy returned null");

        auto offset = vfct->vbiosImageOffset;

        while (offset < vfctData->getLength()) {
            auto *vHdr =
                static_cast<const GOPVideoBIOSHeader *>(vfctData->getBytesNoCopy(offset, sizeof(GOPVideoBIOSHeader)));
            if (!vHdr) {
                DBGLOG("lred", "VFCT header out of bounds");
                return false;
            }

            auto *vContent = static_cast<const uint8_t *>(
                vfctData->getBytesNoCopy(offset + sizeof(GOPVideoBIOSHeader), vHdr->imageLength));
            if (!vContent) {
                DBGLOG("lred", "VFCT VBIOS image out of bounds");
                return false;
            }

            offset += sizeof(GOPVideoBIOSHeader) + vHdr->imageLength;

            if (vHdr->imageLength && vHdr->pciBus == obj->getBusNumber() && vHdr->pciDevice == obj->getDeviceNumber() &&
                vHdr->pciFunction == obj->getFunctionNumber() &&
                vHdr->vendorID == obj->configRead16(kIOPCIConfigVendorID) &&
                vHdr->deviceID == obj->configRead16(kIOPCIConfigDeviceID)) {
                if (!checkAtomBios(vContent, vHdr->imageLength)) {
                    DBGLOG("lred", "VFCT VBIOS is not an ATOMBIOS");
                    return false;
                }
                this->vbiosData = OSData::withBytes(vContent, vHdr->imageLength);
                PANIC_COND(!this->vbiosData, "lred", "VFCT OSData::withBytes failed");
                obj->setProperty("ATY,bin_image", this->vbiosData);
                return true;
            }
        }

        return false;
    }

    bool getVBIOSFromVRAM(IOPCIDevice *provider) {
        auto *bar0 = provider->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
        if (!bar0 || !bar0->getLength()) {
            DBGLOG("lred", "FB BAR not enabled");
            OSSafeReleaseNULL(bar0);
            return false;
        }
        auto *fb = reinterpret_cast<const uint8_t *>(bar0->getVirtualAddress());
        uint32_t size = 256 * 1024;    // ???
        if (!checkAtomBios(fb, size)) {
            DBGLOG("lred", "VRAM VBIOS is not an ATOMBIOS");
            OSSafeReleaseNULL(bar0);
            return false;
        }
        this->vbiosData = OSData::withBytes(fb, size);
        PANIC_COND(!this->vbiosData, "lred", "VRAM OSData::withBytes failed");
        provider->setProperty("ATY,bin_image", this->vbiosData);
        OSSafeReleaseNULL(bar0);
        return true;
    }

    uint32_t readReg32(uint32_t reg) {
        if (reg * 4 < this->rmmio->getLength()) {
            return this->rmmioPtr[reg];
        } else {
            this->rmmioPtr[mmPCIE_INDEX2] = reg;
            return this->rmmioPtr[mmPCIE_DATA2];
        }
    }

    void writeReg32(uint32_t reg, uint32_t val) {
        if (reg * 4 < this->rmmio->getLength()) {
            this->rmmioPtr[reg] = val;
        } else {
            this->rmmioPtr[mmPCIE_INDEX2] = reg;
            this->rmmioPtr[mmPCIE_DATA2] = val;
        }
    }

    template<typename T>
    T *getVBIOSDataTable(uint32_t index) {
        auto *vbios = static_cast<const uint8_t *>(this->vbiosData->getBytesNoCopy());
        auto base = *reinterpret_cast<const uint16_t *>(vbios + ATOM_ROM_TABLE_PTR);
        auto dataTable = *reinterpret_cast<const uint16_t *>(vbios + base + ATOM_ROM_DATA_PTR);
        auto *mdt = reinterpret_cast<const uint16_t *>(vbios + dataTable + 4);

        if (mdt[index]) {
            uint32_t offset = index * 2 + 4;
            auto index = *reinterpret_cast<const uint16_t *>(vbios + dataTable + offset);
            return reinterpret_cast<T *>(const_cast<uint8_t *>(vbios) + index);
        }

        return nullptr;
    }

    OSData *vbiosData {nullptr};
    ChipType chipType = ChipType::Unknown;
    ChipVariant chipVariant = ChipVariant::Normal;
    GFXVersion gfxVer = GFXVersion::Unknown;
    uint64_t fbOffset {0};
    IOMemoryMap *rmmio {nullptr};
    volatile uint32_t *rmmioPtr {nullptr};
    uint32_t deviceId {0};
    uint16_t enumeratedRevision {0};
    uint16_t revision {0};
    IOPCIDevice *iGPU {nullptr};

    void *hwAlignMgr {nullptr};
    uint8_t *hwAlignMgrVtX5000 {nullptr};
    uint8_t *hwAlignMgrVtX6000 {nullptr};

    OSMetaClass *metaClassMap[4][2] = {{nullptr}};

    mach_vm_address_t orgSafeMetaCast {0};
    static OSMetaClassBase *wrapSafeMetaCast(const OSMetaClassBase *anObject, const OSMetaClass *toMeta);

    mach_vm_address_t orgApplePanelSetDisplay {0};

    static size_t wrapFunctionReturnZero();
    static bool wrapApplePanelSetDisplay(IOService *that, IODisplay *display);

    mach_vm_address_t orgCsValidatePage {0};
    static void csValidatePage(vnode *vp, memory_object_t pager, memory_object_offset_t page_offset, const void *data,
        int *validated_p, int *tainted_p, int *nx_p);
};

#endif /* AMDRadeonX6000_hpp */
