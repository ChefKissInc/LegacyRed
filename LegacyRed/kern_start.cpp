//  Copyright © 2022-2023 ChefKiss Inc. Licensed under the Thou Shalt Not Profit License version 1.0. See LICENSE for
//  details.

#include "kern_lred.hpp"
#include <Headers/kern_api.hpp>
#include <Headers/kern_version.hpp>
#include <Headers/plugin_start.hpp>
#include <IOKit/IOCatalogue.h>

static LRed lred;

static const char *bootargOff[] = {
    "-lredoff",
};

static const char *bootargDebug[] = {
    "-lreddbg",
};

static const char *bootargBeta[] = {
    "-lredbeta",
};

PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery | LiluAPI::AllowSafeMode,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::HighSierra,    // we cannot go older, AMD patches only support HS+
    KernelVersion::Monterey,      // we don't actually support Monterey, here for debugging/testing
    []() { lred.init(); },
};

OSDefineMetaClassAndStructors(PRODUCT_NAME, IOService);

IOService *PRODUCT_NAME::probe(IOService *provider, SInt32 *score) {
    setProperty("VersionInfo", kextVersion);
    auto service = IOService::probe(provider, score);
    return ADDPR(startSuccess) ? service : nullptr;
}

bool PRODUCT_NAME::start(IOService *provider) {
    if (!IOService::start(provider)) {
        SYSLOG("init", "Failed to start the parent");
        return false;
    }

    if (!(lilu.getRunMode() & LiluAPI::RunningInstallerRecovery) && ADDPR(startSuccess)) {
        auto *prop = OSDynamicCast(OSArray, this->getProperty("Drivers"));
        if (!prop) {
            SYSLOG("init", "Failed to get Drivers property");
            return false;
        }
        auto *propCopy = prop->copyCollection();
        if (!propCopy) {
            SYSLOG("init", "Failed to copy Drivers property");
            return false;
        }
        auto *drivers = OSDynamicCast(OSArray, propCopy);
        if (!drivers) {
            SYSLOG("init", "Failed to cast Drivers property");
            OSSafeReleaseNULL(propCopy);
            return false;
        }
        if (!gIOCatalogue->addDrivers(drivers)) {
            SYSLOG("init", "Failed to add drivers");
            OSSafeReleaseNULL(drivers);
            return false;
        }
        OSSafeReleaseNULL(drivers);
    }

    return ADDPR(startSuccess);
}
