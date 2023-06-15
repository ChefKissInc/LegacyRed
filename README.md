# LegacyRed ![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/NootInc/LegacyRed/main.yml?branch=master&logo=github&style=for-the-badge)

An AMD Legacy iGPU support [Lilu](https://github.com/acidanthera/Lilu) plugin.

The Source Code of this Derivative Work is licensed under the `Thou Shalt Not Profit License version 1.0`. See `LICENSE`

Thanks to [Acidanthera](https://github.com/acidanthera) for the [WhateverGreen](https://github.com/acidanthera/WhateverGreen) kern_agdc code used in kern_support

Please do not use the artifacts from the dbg branch unless you know what you are doing.

## FAQ

### Can I have an AMD dGPU installed on the system?

Since the kext isn't functioning yet: None, but in the future, due to how we only use the kexts for GCN 1 through 4, you can install a Vega or Navi card

### How functional is the kext?

As of recent, AMDRadeonX4000 is kernel panicing on createHWInterface on both Volcanic Islands and Caribbean Islands accelerators

### On which macOS versions am I able to use this on?

Big Sur.

## Project Members
- [@Zormeister](https://github.com/Zormeister) | LegacyRed lead, kext developer.

## Credits:

- [@ChefKissInc](https://github.com/ChefKissInc) | NootedRed lead, Linux shitcode analyser and kernel extension developer. Extensive knowledge of OS inner workings
- [@NyanCatTW1](https://github.com/NyanCatTW1) | Reverse Engineering and Python automation magician. His Ghidra RedMetaClassAnalyzer script has made the entire process way painless by automagically discovering C++ v-tables for classes.
