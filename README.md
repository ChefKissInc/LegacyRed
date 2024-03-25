# LegacyRed ![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/NootInc/LegacyRed/main.yml?branch=master&logo=github&style=for-the-badge)

An AMD Legacy iGPU support [Lilu](https://github.com/acidanthera/Lilu) plugin.

The LegacyRed derivative work/project is licensed under the `Thou Shalt Not Profit License version 1.5`. See `LICENSE`

Thanks to [Acidanthera](https://github.com/acidanthera) for the [WhateverGreen](https://github.com/acidanthera/WhateverGreen) kern_agdc and kern_rad code used in kern_support

You almost certainly need to [add this custom lilu build to your system](https://github.com/Zormeister/Lilu) to avoid kernel panics on 'No iGPU' as for some reason, Lilu's quirk **fails to trigger on older systems**

## FAQ

### Which iGPUs does this kext support?

It supports Kaveri, Kabini, Mullins, Carrizo and Stoney based iGPUs, aka, GCN 2 and GCN 3.

### Can I have an AMD dGPU installed on the system?

Since the kext isn't functioning yet: None, but in the future, due to how we only use the kexts for GCN 1 through 4, you can install a Vega or Navi card

### How functional is the kext?

Great news, FB-Only is confirmed to work on Kabini and Kaveri APUs, the Accelerator is still broken so stay tuned for future updates

### On which macOS versions am I able to use this on?

The plan is to support as old High Sierra, if we don't end up having to re-work all our patches for 

Now for Ventura and onwards we run into a problem

Fiji, Tonga, Hawaii and Bonaire got dropped by macOS, so how do we support them all?
Unfortunately for Kaveri, Kabini, Mullins and Carrizo, this is where your support ends.
We do NOT condone using OCLP and any issues filed and encountered while using it will be disregarded.

Stoney **will** be worked on for Ventura, as we use Ellesmere's Graphics Accelerator on those devices.

For now, we only test using Big Sur.
