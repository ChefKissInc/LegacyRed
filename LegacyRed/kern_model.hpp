//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
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
/** TODO: Add Model names for Kaveri+ */
static constexpr Model dev15D8[] = {
    {0x00, "AMD Radeon Vega 8 WS"},
    {0x91, "AMD Radeon Vega 3"},
    {0x91, "AMD Radeon Vega Gfx"},
    {0x92, "AMD Radeon Vega 3"},
    {0x92, "AMD Radeon Vega Gfx"},
    {0x93, "AMD Radeon Vega 1"},
    {0xA1, "AMD Radeon Vega 10"},
    {0xA2, "AMD Radeon Vega 8"},
    {0xA3, "AMD Radeon Vega 6"},
    {0xA4, "AMD Radeon Vega 3"},
    {0xB1, "AMD Radeon Vega 10"},
    {0xB2, "AMD Radeon Vega 8"},
    {0xB3, "AMD Radeon Vega 6"},
    {0xB4, "AMD Radeon Vega 3"},
    {0xC1, "AMD Radeon Vega 10"},
    {0xC2, "AMD Radeon Vega 8"},
    {0xC3, "AMD Radeon Vega 6"},
    {0xC4, "AMD Radeon Vega 3"},
    {0xC5, "AMD Radeon Vega 3"},
    {0xC8, "AMD Radeon Vega 11"},
    {0xC9, "AMD Radeon Vega 8"},
    {0xCA, "AMD Radeon Vega 11"},
    {0xCB, "AMD Radeon Vega 8"},
    {0xCC, "AMD Radeon Vega 3"},
    {0xCE, "AMD Radeon Vega 3"},
    {0xCF, "AMD Radeon Vega Gfx"},
    {0xD1, "AMD Radeon Vega 10"},
    {0xD2, "AMD Radeon Vega 8"},
    {0xD3, "AMD Radeon Vega 6"},
    {0xD4, "AMD Radeon Vega 3"},
    {0xD8, "AMD Radeon Vega 11"},
    {0xD9, "AMD Radeon Vega 8"},
    {0xDA, "AMD Radeon Vega 11"},
    {0xDB, "AMD Radeon Vega 3"},
    {0xDB, "AMD Radeon Vega 8"},
    {0xDC, "AMD Radeon Vega 3"},
    {0xDD, "AMD Radeon Vega 3"},
    {0xDE, "AMD Radeon Vega 3"},
    {0xDF, "AMD Radeon Vega 3"},
    {0xE3, "AMD Radeon Vega 3"},
    {0xE4, "AMD Radeon Vega Gfx"},
};

static constexpr Model dev15DD[] = {
    {0x81, "AMD Radeon Vega Gfx"},
    {0x82, "AMD Radeon Vega Gfx"},
    {0x83, "AMD Radeon Vega Gfx"},
    {0x84, "AMD Radeon Vega 6"},
    {0x85, "AMD Radeon Vega Gfx"},
    {0x86, "AMD Radeon Vega 11"},
    {0x88, "AMD Radeon Vega 8"},
    {0xC1, "AMD Radeon Vega 11"},
    {0xC2, "AMD Radeon Vega 8"},
    {0xC3, "AMD Radeon Vega 3/10"},
    {0xC4, "AMD Radeon Vega 8"},
    {0xC5, "AMD Radeon Vega 3"},
    {0xC6, "AMD Radeon Vega 11"},
    {0xC8, "AMD Radeon Vega 8"},
    {0xC9, "AMD Radeon Vega 11"},
    {0xCA, "AMD Radeon Vega 8"},
    {0xCB, "AMD Radeon Vega 3"},
    {0xCC, "AMD Radeon Vega 6"},
    {0xCE, "AMD Radeon Vega 3"},
    {0xCF, "AMD Radeon Vega 3"},
    {0xD0, "AMD Radeon Vega 10"},
    {0xD1, "AMD Radeon Vega 8"},
    {0xD3, "AMD Radeon Vega 11"},
    {0xD5, "AMD Radeon Vega 8"},
    {0xD6, "AMD Radeon Vega 11"},
    {0xD7, "AMD Radeon Vega 8"},
    {0xD8, "AMD Radeon Vega 3"},
    {0xD9, "AMD Radeon Vega 6"},
    {0xE1, "AMD Radeon Vega 3"},
    {0xE2, "AMD Radeon Vega 3"},
};

static constexpr DevicePair devices[] = {
    {0x15D8, dev15D8, arrsize(dev15D8)},
    {0x15DD, dev15DD, arrsize(dev15DD)},
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
        case 0x15D8:
            return "AMD Radeon Vega (Picasso)";
        case 0x15DD:
            return "AMD Radeon Vega (Raven Ridge)";
        case 0x164C:
            return "AMD Radeon Vega (Lucienne)";
        case 0x1636:
            return "AMD Radeon Vega (Renoir)";
        case 0x15E7:
            return "AMD Radeon Vega (Barcelo)";
        case 0x1638:
            return "AMD Radeon Vega (Cezanne)";
        default:
            return nullptr;
    }
}

#endif /* kern_model_hpp */
