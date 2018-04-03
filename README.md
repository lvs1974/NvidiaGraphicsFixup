NvidiaGraphicsFixup
===================

An open source kernel extension providing patches for NVidia GPUs.

#### Features
- Fixes an issue in AppleGraphicsDevicePolicy.kext so that we could use a MacPro6,1 board-id/model combination, 
  without the usual hang with a black screen. 
  [Patching AppleGraphicsDevicePolicy.kext](https://pikeralpha.wordpress.com/2015/11/23/patching-applegraphicsdevicepolicy-kext)
- Modifies macOS to recognize NVIDIA's web drivers as platform binaries. This resolves the issue with transparent windows without content,
  which appear for applications that use Metal and have Library Validation enabled. Common affected applications are iBooks and Little Snitch Network Monitor,
  though this patch is universal and fixes them all.
  [NVWebDriverLibValFix](https://github.com/mologie/NVWebDriverLibValFix)
- Injects IOVARendererID into GPU properties (required for Shiki-based solution for non-freezing Intel and/or any discrete GPU)
- Allows to use ports HDMI, DP, Digital DVI with audio (Injects @0connector-type - @5connector-type properties into GPU)
- Fixes interface stuttering on 10.13 (official and web drivers)

#### Boot-args
- `-ngfxoff` disables kext loading
- `-ngfxdbg` turns on debugging output
- `-ngfxbeta` enables loading under unsupported osx version
- `-ngfxnoaudio` disables all audio fixes
- `-ngfxnoaudiocon` disables audio connectors injection
- `-ngfxnovarenderer` disables IOVARenderer injection
- `-ngfxlibvalfix` disables NVWebDriverLibValFix fix
- `ngfxpatch=cfgmap` enforcing `none` into ConfigMap dictionary for system board-id
- `ngfxpatch=vit9696` disables check for board-id , enabled by default
- `ngfxpatch=pikera` replaces `board-id` with `board-ix`
- `ngfxgl=1` boot argument (and `disable-metal` property) to disable Metal support
- `ngfxcompat=1` boot argument (and `force-compat` property) to ignore compatibility check in NVDAStartupWeb
- `ngfxsubmit=0` boot argument to disable interface stuttering fix on 10.13

#### Credits
- [Apple](https://www.apple.com) for macOS  
- [vit9696](https://github.com/vit9696) for [Lilu.kext](https://github.com/vit9696/Lilu) & for zero-length string comparison patch (AppleGraphicsDevicePolicy.kext )
- [Pike R. Alpha](https://github.com/Piker-Alpha) for board-id patch (AppleGraphicsDevicePolicy.kext)
- [FredWst](http://www.insanelymac.com/forum/user/509660-fredwst/)
- [igork](https://applelife.ru/members/igork.564) for adding properties IOVARendererID & IOVARendererSubID in nvAcceleratorParent::SetAccelProperties
- [mologie](https://github.com/mologie/NVWebDriverLibValFix) for creating NVWebDriverLibValFix.kext which forces macOS to recognize NVIDIA's web drivers as platform binaries
- [lvs1974](https://applelife.ru/members/lvs1974.53809) for writing the software and maintaining it


[FAQ](https://github.com/lvs1974/NvidiaGraphicsFixup/blob/master/FAQ.md)
