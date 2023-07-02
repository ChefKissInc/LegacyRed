//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_vbios_hpp
#define kern_vbios_hpp
#include <Headers/kern_util.hpp>

struct VFCT {
    char signature[4];
    uint32_t length;
    uint8_t revision, checksum;
    char oemId[6];
    char oemTableId[8];
    uint32_t oemRevision;
    char creatorId[4];
    uint32_t creatorRevision;
    char tableUUID[16];
    uint32_t vbiosImageOffset, lib1ImageOffset;
    uint32_t reserved[4];
} PACKED;

struct GOPVideoBIOSHeader {
    uint32_t pciBus, pciDevice, pciFunction;
    uint16_t vendorID, deviceID;
    uint16_t ssvId, ssId;
    uint32_t revision, imageLength;
} PACKED;

struct ATOMCommonTableHeader {
    uint16_t structureSize;
    uint8_t formatRev;
    uint8_t contentRev;
} PACKED;

constexpr uint32_t ATOM_ROM_TABLE_PTR = 0x48;
constexpr uint32_t ATOM_ROM_DATA_PTR = 0x20;

struct IGPSystemInfoV11 : public ATOMCommonTableHeader {
    uint32_t vbiosMisc;
    uint32_t gpuCapInfo;
    uint32_t systemConfig;
    uint32_t cpuCapInfo;
    uint16_t gpuclkSsPercentage;
    uint16_t gpuclkSsType;
    uint16_t lvdsSsPercentage;
    uint16_t lvdsSsRate10hz;
    uint16_t hdmiSsPercentage;
    uint16_t hdmiSsRate10hz;
    uint16_t dviSsPercentage;
    uint16_t dviSsRate10hz;
    uint16_t dpPhyOverride;
    uint16_t lvdsMisc;
    uint16_t backlightPwmHz;
    uint8_t memoryType;
    uint8_t umaChannelCount;
} PACKED;

enum DMIT17MemType : uint8_t {
    kDDR2MemType = 0x13,
    kDDR2FBDIMMMemType,
    kDDR3MemType = 0x18,
    kDDR4MemType = 0x1A,
    kLPDDR2MemType = 0x1C,
    kLPDDR3MemType,
    kLPDDR4MemType,
    kDDR5MemType = 0x22,
    kLPDDR5MemType,
};

struct IGPSystemInfoV2 : public ATOMCommonTableHeader {
    uint32_t vbiosMisc;
    uint32_t gpuCapInfo;
    uint32_t systemConfig;
    uint32_t cpuCapInfo;
    uint16_t gpuclkSsPercentage;
    uint16_t gpuclkSsType;
    uint16_t dpPhyOverride;
    uint8_t memoryType;
    uint8_t umaChannelCount;
} PACKED;

union IGPSystemInfo {
    ATOMCommonTableHeader header;
    IGPSystemInfoV11 infoV11;
    IGPSystemInfoV2 infoV2;
};

struct ATOMDispObjPathV2 {
    uint16_t dispObjId;
    uint16_t dispRecordOff;
    uint16_t encoderObjId;
    uint16_t extEncoderObjId;
    uint16_t encoderRecordOff;
    uint16_t extEncoderRecordOff;
    uint16_t devTag;
    uint8_t priorityId;
    uint8_t _reserved;
} PACKED;

struct DispObjInfoTableV1_4 : public ATOMCommonTableHeader {
    uint16_t supportedDevices;
    uint8_t pathCount;
    uint8_t _reserved;
    ATOMDispObjPathV2 paths[];
} PACKED;

struct ATOMConnectorDeviceTag {
    uint32_t ulACPIDeviceEnum;    // Reserved for now
    uint16_t usDeviceID;          // This Id is same as "ATOM_DEVICE_XXX_SUPPORT"
    uint16_t usPadding;
} PACKED;

struct ATOMCommonRecordHeader {
    uint8_t recordType;
    uint8_t recordSize;
} PACKED;

struct ATOMConnectorDeviceTagRecord : public ATOMCommonRecordHeader {
    uint8_t numberOfDevice;
    uint8_t reserved;
    ATOMConnectorDeviceTag deviceTag[];
} PACKED;

struct ATOMSrcDstTable {
    uint8_t ucNumberOfSrc;
    uint16_t usSrcObjectID[1];
    uint8_t ucNumberOfDst;
    uint16_t usDstObjectID[1];
} PACKED;

// Above new ATOM structs are for later.

#endif /* kern_vbios_hpp */
