# LegacyRed ![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/Zorm-Industries/LegacyRed/main.yml?branch=master&logo=github&style=for-the-badge)

An AMD Legacy iGPU support [Lilu](https://github.com/acidanthera/Lilu) plugin.

The Source Code of this Derivative Work is licensed under the `Thou Shalt Not Profit License version 1.0`. See `LICENSE`

## FAQ

### Can I have an AMD dGPU installed on the system?

Since the kext isn't functioning yet: None.

### How functional is the kext?

We're at the very start of working on this, IE: getting everything to power on, and getting all the kexts to load
### On which macOS versions am I able to use this on?

Big Sur.

## Project Members
- [@Zormeister](https://github.com/Zormeister) | LegacyRed lead, kext developer.

## Credits:

- [@ChefKissInc](https://github.com/ChefKissInc) | NootedRed lead, Linux shitcode analyser and kernel extension developer. Extensive knowledge of OS inner workings
- [@NyanCatTW1](https://github.com/NyanCatTW1) | Reverse Engineering and Python automation magician. His Ghidra RedMetaClassAnalyzer script has made the entire process way painless by automagically discovering C++ v-tables for classes.
