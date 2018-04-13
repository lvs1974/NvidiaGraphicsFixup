NvidiaGraphicsFixup Changelog
=============================

#### v1.2.6
- Made `ngfxcompat=0` explicitly disable NVDAStartupWeb.kext loading from HDD
- Disabled AGDC patches for compatible models to avoid issues with IGPU switching on MacBooks

#### v1.2.5
- Hardened solved symbol verification to avoid panics with broken kext cache
- Added a workaround to interface lags in 10.13 ( add `ngfxsubmit=0` boot-argument to disable)
- Fixed improperly working forced driver compatibility from 1.2.4

With this change you may not need to disable Metal support. Make sure to restore CoreDisplay preferences:
```
sudo defaults delete /Library/Preferences/com.apple.CoreDisplay useMetal
sudo defaults delete /Library/Preferences/com.apple.CoreDisplay useIOP
```

#### v1.2.4
- Added `ngfxgl=1` boot argument (and `disable-metal` property) to disable Metal support
- Added `ngfxcompat=1` boot argument (and `force-compat` property) to ignore compatibility check in NVDAStartupWeb

In order to boot with `ngfxgl=1` on 10.13.x you may need to set the defaults:
```
sudo defaults write /Library/Preferences/com.apple.CoreDisplay useMetal -boolean no
sudo defaults write /Library/Preferences/com.apple.CoreDisplay useIOP -boolean no
```

#### v1.2.3
- Add AAPL,slot-name injection
- Fix HDEF `layout-id` detection

#### v1.2.2
- Fix up will be loaded in safe mode (required to fix black screen)

#### v1.2.1
- All patches can be turned off by boot-args (and some of them can be also turned off by using ioreg properties)

#### v1.2.0
- Lilu 1.2.0 compatibility fixes
- NVidiaAudio device to add connector-type, layout-id and other properties for HDMI audio (allows audio for HDMI, DP, Digital DVI ports)
- A new hook for nvAcceleratorParent::SetAccelProperties to add properties "IOVARendererID" and "IOVARendererSubID"
- NVWebDriverLibValFix fix is implemented (csfg_get_platform_binary)

#### v1.1.3
- High Sierra compatibility

#### v1.1.2
- Added OSBundleCompatibleVersion

#### v1.1
- Patch has been improved (vit9696)

#### v1.0.0
- Initial release
