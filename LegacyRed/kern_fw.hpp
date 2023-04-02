//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#ifndef kern_fw_hpp
#define kern_fw_hpp
#include <Headers/kern_util.hpp>
#include <libkern/c++/OSData.h>

struct FwDesc {
    const char *name;
    const uint8_t *data;
    const uint32_t size;
};

#define LRED_FW(fw_name, fw_data, fw_size) .name = fw_name, .data = fw_data, .size = fw_size

extern const struct FwDesc fwList[];
extern const int fwNumber;

inline const FwDesc &getFWDescByName(const char *name) {
    for (int i = 0; i < fwNumber; i++) {
        if (strcmp(fwList[i].name, name) == 0) { return fwList[i]; }
    }
    PANIC("lred", "getFWDescByName: '%s' not found", name);
}

#endif /* kern_fw_hpp */
