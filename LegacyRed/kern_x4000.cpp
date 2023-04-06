//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_x4000.hpp"
#include "kern_lred.hpp"
#include "kern_patches.hpp"
#include <Headers/kern_api.hpp>

static const char *pathRadeonX4000 = "/System/Library/Extensions/AMDRadeonX4000.kext/Contents/MacOS/AMDRadeonX4000";

static KernelPatcher::KextInfo kextRadeonX4000 {"com.apple.kext.AMDRadeonX4000", &pathRadeonX4000, 1, {}, {},
    KernelPatcher::KextInfo::Unloaded};

X4000 *X4000::callback = nullptr;

void X4000::init() {
    callback = this;
    lilu.onKextLoadForce(&kextRadeonX4000);
}

bool X4000::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    if (kextRadeonX4000.loadIndex == index) {
        uint32_t *orgChannelTypes = nullptr;

        KernelPatcher::SolveRequest solveRequests[] = {
            {"__ZN29AMDRadeonX4000_AMDCIPM4EngineC1Ev", this->orgGFX7PM4EngineConstructor},
            {"__ZN30AMDRadeonX4000_AMDCIsDMAEngineC1Ev", this->orgGFX7SDMAEngineConstructor},
			{"__ZN31AMDRadeonX4000_AMDCIUVDHWEngineC1Ev", this->orgGFX7UVDEngineConstructor},
			{"__ZN30AMDRadeonX4000_AMDCISAMUEngineC1Ev", this->orgGFX7SAMUEngineConstructor},
			{"__ZN31AMDRadeonX4000_AMDCIVCEHWEngineC1Ev", this->orgGFX7VCEEngineConstructor},
			{"__ZN29AMDRadeonX4000_AMDVIPM4EngineC1Ev", this->orgGFX8PM4EngineConstructor},
			{"__ZN30AMDRadeonX4000_AMDVIsDMAEngineC1Ev", this->orgGFX8SDMAEngineConstructor},
			{"__ZN31AMDRadeonX4000_AMDVIUVDHWEngineC1Ev", this->orgGFX8UVDEngineConstructor},
			{"__ZN30AMDRadeonX4000_AMDVISAMUEngineC1Ev", this->orgGFX8SAMUEngineConstructor},
			{"__ZN31AMDRadeonX4000_AMDVIVCEHWEngineC1Ev", this->orgGFX8VCEEngineConstructor},
            {"__ZZN37AMDRadeonX4000_AMDGraphicsAccelerator19createAccelChannelsEbE12channelTypes", orgChannelTypes},
            {"__ZN28AMDRadeonX4000_AMDCIHardware32setupAndInitializeHWCapabilitiesEv",
                this->orgSetupAndInitializeHWCapabilities},
			{"__ZN28AMDRadeonX4000_AMDVIHardware32setupAndInitializeHWCapabilitiesEv",
				this->orgSetupAndInitializeHWCapabilities},
        };
        PANIC_COND(!patcher.solveMultiple(index, solveRequests, address, size), "x5000", "Failed to resolve symbols");

        KernelPatcher::RouteRequest requests[] = {
			{"__ZN37AMDRadeonX4000_AMDGraphicsAccelerator5startEP9IOService", wrapAccelStart, orgAccelStart},
            {"__ZN28AMDRadeonX4000_AMDVIHardware17allocateHWEnginesEv", wrapAllocateHWEngines},
			{"__ZN28AMDRadeonX4000_AMDCIHardware17allocateHWEnginesEv", wrapAllocateHWEngines},
            {"__ZN28AMDRadeonX4000_AMDCIHardware32setupAndInitializeHWCapabilitiesEv", wrapSetupAndInitializeHWCapabilities},
			{"__ZN28AMDRadeonX4000_AMDVIHardware32setupAndInitializeHWCapabilitiesEv", wrapSetupAndInitializeHWCapabilities},
            {"__ZN28AMDRadeonX4000_AMDCIHardware20initializeFamilyTypeEv", wrapInitializeFamilyType},
			{"__ZN28AMDRadeonX4000_AMDVIHardware20initializeFamilyTypeEv", wrapInitializeFamilyType},
        };
        PANIC_COND(!patcher.routeMultiple(index, requests, address, size), "x4000", "Failed to route symbols");

        // PANIC_COND(MachInfo::setKernelWriting(true, KernelPatcher::kernelWriteLock) != KERN_SUCCESS, "x5000",
        //  "Failed to enable kernel writing");
        /** TODO: Port this */
        // orgChannelTypes[5] = 1;     // Fix createAccelChannels so that it only starts SDMA0
        // orgChannelTypes[11] = 0;    // Fix getPagingChannel so that it gets SDMA0
        // MachInfo::setKernelWriting(false, KernelPatcher::kernelWriteLock);
        // DBGLOG("x4000", "Applied SDMA1 patches");

        // KernelPatcher::LookupPatch patch = {&kextRadeonX5000, kStartHWEnginesOriginal, kStartHWEnginesPatched,
        // arrsize(kStartHWEnginesOriginal), 1};
        // patcher.applyLookupPatch(&patch);
        // patcher.clearError();

        return true;
    }

    return false;
}

bool X4000::wrapAccelStart(void *that, IOService *provider) {
    DBGLOG("x4000", "accelStart: this = %p provider = %p", that, provider);
    callback->callbackAccelerator = that;
    auto ret = FunctionCast(wrapAccelStart, callback->orgAccelStart)(that, provider);
    DBGLOG("x4000", "accelStart returned %d", ret);
    return ret;
}

/** Rough calculations based on AMDRadeonX4000's Assembly  */
bool X4000::wrapAllocateHWEngines(void *that) {
	if (LRed::callback->gfxVer == GFXVersion::GFX7) {
		DBGLOG("x4000", "Using GFX7 Constructors");
		callback->orgGFX7PM4EngineConstructor(getMember<void *>(that, 0x3B0) = IOMallocZero(0x198));
		callback->orgGFX7SDMAEngineConstructor(getMember<void *>(that, 0x3B8) = IOMallocZero(0x118));
		callback->orgGFX7SDMAEngineConstructor(getMember<void *>(that, 0x3C0) = IOMallocZero(0x118));
		callback->orgGFX7UVDEngineConstructor(getMember<void *>(that, 0x3D8) = IOMallocZero(0x2F0));
		callback->orgGFX7SAMUEngineConstructor(getMember<void *>(that, 0x400) = IOMallocZero(0x1C8));
		callback->orgGFX7VCEEngineConstructor(getMember<void *>(that, 0x3E8) = IOMallocZero(0x258));
	} else if (LRed::callback->gfxVer == GFXVersion::GFX8) {
		DBGLOG("x4000", "Using GFX8 Constructors");
		callback->orgGFX8PM4EngineConstructor(getMember<void *>(that, 0x3B0) = IOMallocZero(0x198));
		callback->orgGFX8SDMAEngineConstructor(getMember<void *>(that, 0x3B8) = IOMallocZero(0x100));
		/** Only one SDMA channel is present on Stoney APUs  */
		if (LRed::callback->chipType == ChipType::Stoney) {
			DBGLOG("x4000", "Using only 1 SDMA Engine for Stoney");
		} else {
			callback->orgGFX8SDMAEngineConstructor(getMember<void *>(that, 0x3C0) = IOMallocZero(0x100));
		}
		callback->orgGFX8UVDEngineConstructor(getMember<void *>(that, 0x3D8) = IOMallocZero(0x2F0));
		callback->orgGFX8SAMUEngineConstructor(getMember<void *>(that, 0x400) = IOMallocZero(0x1D0));
		callback->orgGFX8VCEEngineConstructor(getMember<void *>(that, 0x3E8) = IOMallocZero(0x258));
	} else if (LRed::callback->gfxVer == GFXVersion::Unknown) {
		PANIC("lred", "GFX Version is Unknown!");
	}

    return true;
}
// Likely to be unused, here for incase we need to use it for X4000::setupAndInitializeHWCapabilities
enum HWCapability : uint64_t {
    DisplayPipeCount = 0x04,    // uint32_t
    SECount = 0x34,             // uint32_t
    SHPerSE = 0x3C,             // uint32_t
    CUPerSH = 0x70,             // uint32_t
    HasUVD0 = 0x84,             // bool
    HasUVD1 = 0x85,             // bool
    HasVCE = 0x86,              // bool
    HasVCN0 = 0x87,             // bool
    HasVCN1 = 0x88,             // bool
    HasHDCP = 0x8D,             // bool
    HasSDMAPageQueue = 0x98,    // bool
};
