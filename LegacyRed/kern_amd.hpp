//  Copyright © 2022 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

//  Copyright © 2023 Zormeister. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_amd_hpp
#define kern_amd_hpp
#include <Headers/kern_util.hpp>

using t_GenericConstructor = void (*)(void *that);
using t_sendMsgToSmc = uint32_t (*)(void *smum, uint32_t msgId);
using t_pspLoadExtended = uint32_t (*)(void *, uint64_t, uint64_t, const void *, size_t);

constexpr uint32_t AMDGPU_FAMILY_CZ = 0x87;
constexpr uint32_t AMDGPU_FAMILY_KV = 0x7D;

constexpr uint32_t PPSMC_MSG_GetSmuVersion = 0x2;
constexpr uint32_t PPSMC_MSG_PowerUpSdma = 0xE;
constexpr uint32_t PPSMC_MSG_PowerGateMmHub = 0x35;

constexpr uint32_t GFX_FW_TYPE_CP_MEC = 5;
constexpr uint32_t GFX_FW_TYPE_CP_MEC_ME2 = 6;

constexpr uint32_t MP_BASE = 0x16000;

constexpr uint32_t AMDGPU_MAX_USEC_TIMEOUT = 100000;

constexpr uint32_t mmMP1_SMN_C2PMSG_90 = 0x29A;
constexpr uint32_t mmMP1_SMN_C2PMSG_82 = 0x292;
constexpr uint32_t mmMP1_SMN_C2PMSG_66 = 0x282;

constexpr uint32_t mmPCIE_INDEX2 = 0xE;
constexpr uint32_t mmPCIE_DATA2 = 0xF;

struct CommonFirmwareHeader {
    uint32_t size;
    uint32_t headerSize;
    uint16_t headerMajor;
    uint16_t headerMinor;
    uint16_t ipMajor;
    uint16_t ipMinor;
    uint32_t ucodeVer;
    uint32_t ucodeSize;
    uint32_t ucodeOff;
    uint32_t crc32;
} PACKED;

struct GfxFwHeaderV1 : public CommonFirmwareHeader {
    uint32_t ucodeFeatureVer;
    uint32_t jtOff;
    uint32_t jtSize;
} PACKED;

struct SdmaFwHeaderV1 : public CommonFirmwareHeader {
    uint32_t ucodeFeatureVer;
    uint32_t ucodeChangeVer;
    uint32_t jtOff;
    uint32_t jtSize;
} PACKED;

struct RlcFwHeaderV2_1 : public CommonFirmwareHeader {
    uint32_t ucodeFeatureVer;
    uint32_t jtOff;
    uint32_t jtSize;
    uint32_t saveAndRestoreOff;
    uint32_t clearStateDescOff;
    uint32_t availScratchRamLocations;
    uint32_t regRestoreListSize;
    uint32_t regListFmtStart;
    uint32_t regListFmtSeparateStart;
    uint32_t startingOffsetsStart;
    uint32_t regListFmtSize;
    uint32_t regListFmtArrayOff;
    uint32_t regListSize;
    uint32_t regListArrayOff;
    uint32_t regListFmtSeparateSize;
    uint32_t regListFmtSeparateArrayOff;
    uint32_t regListSeparateSize;
    uint32_t regListSeparateArrayOff;
    uint32_t regListFmtDirectRegListLen;
    uint32_t saveRestoreListCntlUcodeVer;
    uint32_t saveRestoreListCntlFeatureVer;
    uint32_t saveRestoreListCntlSize;
    uint32_t saveRestoreListCntlOff;
    uint32_t saveRestoreListGpmUcodeVer;
    uint32_t saveRestoreListGpmFeatureVer;
    uint32_t saveRestoreListGpmSize;
    uint32_t saveRestoreListGpmOff;
    uint32_t saveRestoreListSrmUcodeVer;
    uint32_t saveRestoreListSrmFeatureVer;
    uint32_t saveRestoreListSrmSize;
    uint32_t saveRestoreListSrmOff;
} PACKED;

struct PspFwLegacyBinDesc {
    uint32_t fwVer;
    uint32_t off;
    uint32_t size;
} PACKED;

struct PspTaFwHeaderV1 : public CommonFirmwareHeader {
    PspFwLegacyBinDesc xgmi, ras, hdcp, dtm, securedisplay;
} PACKED;

struct GcFwConstant {
    const char *firmwareVer;
    uint32_t featureVer, size;
    uint32_t addr, unknown4;
    uint32_t unknown5, unknown6;
    const uint8_t *data;
} PACKED;

struct SdmaFwConstant {
    const char *unknown1;
    uint32_t size, unknown2;
    const uint8_t *data;
} PACKED;

struct GPUInfoFirmware {
    uint32_t gcNumSe;
    uint32_t gcNumCuPerSh;
    uint32_t gcNumShPerSe;
    uint32_t gcNumRbPerSe;
    uint32_t gcNumTccs;
    uint32_t gcNumGprs;
    uint32_t gcNumMaxGsThds;
    uint32_t gcGsTableDepth;
    uint32_t gcGsPrimBuffDepth;
    uint32_t gcParameterCacheDepth;
    uint32_t gcDoubleOffchipLdsBuffer;
    uint32_t gcWaveSize;
    uint32_t gcMaxWavesPerSimd;
    uint32_t gcMaxScratchSlotsPerCu;
    uint32_t gcLdsSize;
} PACKED;

struct CailAsicCapEntry {
    uint32_t familyId, deviceId;
    uint32_t revision, emulatedRev;
    uint32_t pciRev;
    uint32_t _reserved;
    const uint32_t *caps;
    const uint32_t *skeleton;
} PACKED;

struct CailInitAsicCapEntry {
    uint64_t familyId, deviceId;
    uint64_t revision, emulatedRev;
    uint64_t pciRev;
    const uint32_t *caps;
    const void *goldenCaps;
} PACKED;
/** TODO: Port */
/*
static const uint32_t ddiCapsRaven[16] = {0x800005U, 0x500011FEU, 0x80000U, 0x11001000U, 0x200U, 0x68000001U,
        0x20000000, 0x4002U, 0x22420001U, 0x9E20E10U, 0x2000120U, 0x0U, 0x0U, 0x0U, 0x0U, 0x0U};
static const uint32_t ddiCapsRenoir[16] = {0x800005U, 0x500011FEU, 0x80000U, 0x11001000U, 0x200U, 0x68000001U,
        0x20000000, 0x4002U, 0x22420001U, 0x9E20E18U, 0x2000120U, 0x0U, 0x0U, 0x0U, 0x0U, 0x0U};
*/
enum AMDReturn : uint32_t {
    kAMDReturnSuccess = 0,
    kAMDReturnInvalidArgument,
    kAMDReturnGeneralFailure,
    kAMDReturnResourcesExhausted,
    kAMDReturnUnsupported,
};

struct CailDeviceTypeEntry {
    uint32_t deviceId;
    uint32_t deviceType;
} PACKED;

#endif /* kern_amd.hpp */
