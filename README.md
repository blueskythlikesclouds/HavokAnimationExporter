# Havok Animation Exporter
This is a tool that allows you to generate skeleton and animation data for Sonic games that use Havok. (It could also work for non-Sonic games.)

# Usage
The tool accepts a source FBX file as input and a destination HKX file as output.

For generating skeleton data, simply presenting the FBX file is sufficient:  
```HavokAnimationExporter chr_Sonic_HD.fbx chr_Sonic_HD.skl.hkx```

For generating animation data, path to skeleton HKX file (prefixed with `--skl`) needs to be presented:  
```HavokAnimationExporter --skl chr_Sonic_HD.skl.hkx sn_idle_loop.fbx sn_idle_loop.anm.hkx```

If no destination path is specified, it's going to be automatically assumed based on input file path.

Alternatively, you can use the BAT files that come with release packages, and edit the path of skeleton HKX file in them as necessary.

# Versions
The tool comes in many Havok SDK versions. Currently, 3 SDKs are implemented:

## Havok 5.5.0 (Sonic Unleashed)
Output files are going to work on all platforms.

Before generating animation data, you need to endian swap your skeleton HKX file. You can do it using [this tool made by DarioSamo.](https://docs.google.com/file/d/0B1CzI_WhoSoCdXI1MjdaTlkwR2M/edit) After conversion, you should use the HKX file in `output` folder. (_not_ `asset-cced`) **Be careful not to use the endian swapped HKX file in game accidentally.**

## Havok 2010 (Sonic Generations)
Output files are going to work on all platforms, however the tool accepts skeleton HKX files only from the PC version.

## Havok 2012 (Sonic Lost World/Sonic Forces)
Output files are going to work on all platforms.

For Sonic Lost World, the tool accepts skeleton HKX files only from the PC version.  
For Sonic Forces, before generating animation data, you need to backport your skeleton HKX file using [TagTools.](https://github.com/blueskythlikesclouds/TagTools) **Be careful not to use the backported HKX file in game accidentally.**