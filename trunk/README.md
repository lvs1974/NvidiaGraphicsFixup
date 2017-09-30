NvidiaGraphicsFixup
===================

An open source kernel extension providing patches for NVidia GPUs.

#### Features
- Fixes an issue in AppleGraphicsDevicePolicy.kext so that we could use a MacPro6,1 board-id/model combination, 
  without the usual hang with a black screen. 
  (https://pikeralpha.wordpress.com/2015/11/23/patching-applegraphicsdevicepolicy-kext/)
- Modifies macOS to recognize NVIDIA's web drivers as platform binaries. This resolves the issue with transparent windows without content,
  which appear for applications that use Metal and have Library Validation enabled. Common affected applications are iBooks and Little Snitch Network Monitor,
  though this patch is universal and fixes them all.
  (https://github.com/mologie/NVWebDriverLibValFix)
- Injects IOVARendererID into GPU properties (required for Shiki-based solution for non-freezing Intel and/or any discrete GPU)
- Allows to use ports HDMI, DP, Digital DVI with audio (Injects @0connector-type - @5connector-type properties into GPU)

#### Credits
- [Apple](https://www.apple.com) for macOS  
- [vit9696](https://github.com/vit9696) for [Lilu.kext](https://github.com/vit9696/Lilu) & for patch
- [Pike R. Alpha](https://github.com/Piker-Alpha) for AppleGraphicsDevicePolicy.kext patch
- [igork](https://applelife.ru/members/igork.564) for adding properties IOVARendererID & IOVARendererSubID in nvAcceleratorParent::SetAccelProperties
- [mologie](https://github.com/mologie/NVWebDriverLibValFix) for creating NVWebDriverLibValFix.kext which forces macOS to recognize NVIDIA's web drivers as platform binaries
- [lvs1974](https://applelife.ru/members/lvs1974.53809) for writing the software and maintaining it
