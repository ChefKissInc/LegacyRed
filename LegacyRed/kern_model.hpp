//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

//  Copyright © 2023 Zormeister. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_model_hpp
#define kern_model_hpp
#include <Headers/kern_util.hpp>

struct Model {
    uint16_t rev {0};
    const char *name {nullptr};
};

struct DevicePair {
    uint16_t dev;
    const Model *models;
    size_t modelNum;
};

// Kaveri

static constexpr Model dev1309[] = {
    {0x00, "AMD Radeon R7 Graphics"},
};

static constexpr Model dev130A[] = {
    {0x00, "AMD Radeon R6 Graphics"},
};

static constexpr Model dev130B[] = {
    {0x00, "AMD Radeon R4 Graphics"},
}

static constexpr Model dev130C[] = {
    {0x00, "AMD Radeon R7 Graphics"},
};

static constexpr Model dev130D[] = {
    {0x00, "AMD Radeon R6 Graphics"},
};

static constexpr Model dev130E[] = {
    {0x00, "AMD Radeon R5 Graphics"},
};

static constexpr Model dev130F[] = {
    {0x00, "AMD Radeon R7 Graphics"},
    {0xD4, "AMD Radeon R7 Graphics"},
    {0xD5, "AMD Radeon R7 Graphics"},
    {0xD6, "AMD Radeon R7 Graphics"},
    {0xD7, "AMD Radeon R7 Graphics"},
};

static constexpr Model dev1313[] = {
    {0x00, "AMD Radeon R7 Graphics"},
    {0xD4, "AMD Radeon R7 Graphics"},
    {0xD5, "AMD Radeon R7 Graphics"},
    {0xD6, "AMD Radeon R7 Graphics"},
};

static constexpr Model dev1315[] = {
    {0x00, "AMD Radeon R5 Graphics"},
    {0xD4, "AMD Radeon R5 Graphics"},
    {0xD5, "AMD Radeon R5 Graphics"},
    {0xD6, "AMD Radeon R5 Graphics"},
    {0xD7, "AMD Radeon R5 Graphics"},
};

static constexpr Model dev1316[] = {
    {0x00, "AMD Radeon R5 Graphics"},
};

static constexpr Model dev1318[] = {
    {0x00, "AMD Radeon R5 Graphics"},
};

static constexpr Model dev131B[] = {
    {0x00, "AMD Radeon R4 Graphics"},
};

// Kabini

static constexpr Model dev9830[] = {
    {0x00, "AMD Radeon HD 8400 / R3 Series"},
};

static constexpr Model dev9831[] = {
    {0x00, "AMD Radeon HD 8400E"},
};

static constexpr Model dev9832[] = {
    {0x00, "AMD Radeon HD 8330"},
};

static constexpr Model dev9833[] = {
    {0x00, "AMD Radeon HD 8330E"},
};

static constexpr Model dev9834[] = {
    {0x00, "AMD Radeon HD 8210"},
};

static constexpr Model dev9835[] = {
    {0x00, "AMD Radeon HD 8210E"},
};

static constexpr Model dev9836[] = {
    {0x00, "AMD Radeon HD 8200 / R3 Series"},
};

static constexpr Model dev9837[] = {
    {0x00, "AMD Radeon HD 8280E"},
};

static constexpr Model dev9838[] = {
    {0x00, "AMD Radeon HD 8200 / R3 Series"},
};

static constexpr Model dev9839[] = {
    {0x00, "AMD Radeon HD 8180"},
};

static constexpr Model dev983D[] = {
    {0x00, "AMD Radeon HD 8250"},
};

// Mullins

static constexpr Model dev9850[] = {
    {0x00, "AMD Radeon R3 Graphics"},
    {0x03, "AMD Radeon R3 Graphics"},
    {0x40, "AMD Radeon R2 Graphics"},
    {0x45, "AMD Radeon R3 Graphics"},
};

static constexpr Model dev9851[] = {
    {0x00, "AMD Radeon R4 Graphics"},
    {0x01, "AMD Radeon R5E Graphics"},
    {0x05, "AMD Radeon R5 Graphics"},
    {0x06, "AMD Radeon R5E Graphics"},
    {0x40, "AMD Radeon R4 Graphics"},
    {0x45, "AMD Radeon R5 Graphics"},
};

static constexpr Model dev9852[] = {
    {0x00, "AMD Radeon R2 Graphics"},
    {0x40, "AMD Radeon E1 Graphics"},
};

static constexpr Model dev9853[] = {
    {0x00, "AMD Radeon R2 Graphics"},
    {0x01, "AMD Radeon R4E Graphics"},
    {0x03, "AMD Radeon R2 Graphics"},
    {0x05, "AMD Radeon R1E Graphics"},
    {0x06, "AMD Radeon R1E Graphics"},
    {0x07, "AMD Radeon R1E Graphics"},
    {0x08, "AMD Radeon R1E Graphics"},
    {0x40, "AMD Radeon R2 Graphics"},
};

static constexpr Model dev9854[] = {
    {0x00, "AMD Radeon R3 Graphics"},
    {0x01, "AMD Radeon R3E Graphics"},
    {0x02, "AMD Radeon R3 Graphics"},
    {0x05, "AMD Radeon R2 Graphics"},
    {0x06, "AMD Radeon R4 Graohics"},
    {0x07, "AMD Radeon R3 Graphics"},
};

static constexpr Model dev9855[] = {
    {0x02, "AMD Radeon R6 Graphics"},
    {0x05, "AMD Radeon R4 Graphics"},
};

static constexpr Model dev9856[] = {
    {0x00, "AMD Radeon R2 Graphics"},
    {0x01, "AMD Radeon R2E Graphics"},
    {0x02, "AMD Radeon R2 Graphics"},
    {0x05, "AMD Radeon R1E Graphics"},
    {0x06, "AMD Radeon R2 Graphics"},
    {0x07, "AMD Radeon R1E Graphics"},
    {0x08, "AMD Radeon R1E Graphics"},
    {0x13, "AMD Radeon R1E Graphics"},
};

static constexpr Model dev9874 = {
    {0x81, "AMD Radeon R6 Graphics"},
    {0x84, "AMD Radeon R7 Graphics"},
    {0x85, "AMD Radeon R6 Graphics"},
    {0x87, "AMD Radeon R5 Graphics"},
    {0x88, "AMD Radeon R7E Graphics"},
    {0x89, "AMD Radeon R6E Graphics"},
    {0xC4, "AMD Radeon R7 Graphics"},
    {0xC5, "AMD Radeon R6 Graphics"},
    {0xC6, "AMD Radeon R6 Graphics"},
    {0xC7, "AMD Radeon R5 Graphics"},
    {0xC8, "AMD Radeon R7 Graphics"},
    {0xC9, "AMD Radeon R7 Graphics"},
    {0xCA, "AMD Radeon R5 Graphics"},
    {0xCB, "AMD Radeon R5 Graphics"},
    {0xCC, "AMD Radeon R7 Graphics"},
    {0xCD, "AMD Radeon R7 Graphics"},
    {0xCE, "AMD Radeon R5 Graphics"},
    {0xE1, "AMD Radeon R7 Graphics"},
    {0xE2, "AMD Radeon R7 Graphics"},
    {0xE3, "AMD Radeon R7 Graphics"},
    {0xE4, "AMD Radeon R7 Graphics"},
    {0xE5, "AMD Radeon R5 Graphics"},
    {0xE6, "AMD Radeon R5 Graphics"},
};

static constexpr DevicePair devices[] = {
    {0x1309, dev1309, arrsize(dev1309)},
    {0x130A, dev130A, arrsize(dev130A)},
};

inline const char *getBranding(uint16_t dev, uint16_t rev) {
    for (auto &device : devices) {
        if (device.dev == dev) {
            for (size_t i = 0; i < device.modelNum; i++) {
                auto &model = device.models[i];
                if (model.rev == rev) { return model.name; }
            }
            break;
        }
    }

    switch (dev) {
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
            return "AMD Radeon HD 8XXX";
        default:
            return "AMD Radeon R Graphics";
    }
}

#endif /* kern_model_hpp */
