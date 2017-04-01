//
//  kern_ngfx.cpp
//  NvidiaGraphicsFixup
//
//  Copyright © 2017 lvs1974. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <Library/LegacyIOService.h>

#include <mach/vm_map.h>
#include <IOKit/IORegistryEntry.h>

#include "kern_ngfx.hpp"


static const char *kextGraphicsDevicePolicy[] { "/System/Library/Extensions/AppleGraphicsControl.kext/Contents/PlugIns/AppleGraphicsDevicePolicy.kext/Contents/MacOS/AppleGraphicsDevicePolicy" };

static KernelPatcher::KextInfo kextList[] {
	{ "com.apple.driver.AppleGraphicsDevicePolicy", kextGraphicsDevicePolicy, 1, true, {}, KernelPatcher::KextInfo::Unloaded }
};

static size_t kextListSize {1};

bool NGFX::init() {
	LiluAPI::Error error = lilu.onKextLoad(kextList, kextListSize,
	[](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
		NGFX *ngfx = static_cast<NGFX *>(user);
		ngfx->processKext(patcher, index, address, size);
	}, this);
	
	if (error != LiluAPI::Error::NoError) {
		SYSLOG("ngfx @ failed to register onPatcherLoad method %d", error);
		return false;
	}
	
	return true;
}

void NGFX::deinit() {
}

void NGFX::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	if (progressState != ProcessingState::EverythingDone) {
		for (size_t i = 0; i < kextListSize; i++) {
			if (kextList[i].loadIndex == index) {
                if (!(progressState & ProcessingState::GraphicsDevicePolicyPatched) && !strcmp(kextList[i].id, "com.apple.driver.AppleGraphicsDevicePolicy")) {
                    DBGLOG("ngfx @ found com.apple.driver.AppleGraphicsDevicePolicy");
                    const uint8_t find[]    = {0xBA, 0x05, 0x00, 0x00, 0x00};
                    const uint8_t replace[] = {0xBA, 0x00, 0x00, 0x00, 0x00};
                    KextPatch kext_patch {
                        {&kextList[i], find, replace, sizeof(find), 1},
						KernelVersion::MountainLion, KernelVersion::Sierra
					};
                    applyPatches(patcher, index, &kext_patch, 1);
                    progressState |= ProcessingState::GraphicsDevicePolicyPatched;
                }
			}
		}
	}
	
	// Ignore all the errors for other processors
	patcher.clearError();
}

void NGFX::applyPatches(KernelPatcher &patcher, size_t index, const KextPatch *patches, size_t patchNum) {
    DBGLOG("ngfx @ applying patches for %zu kext", index);
    for (size_t p = 0; p < patchNum; p++) {
        auto &patch = patches[p];
        if (patch.patch.kext->loadIndex == index) {
            if (patcher.compatibleKernel(patch.minKernel, patch.maxKernel)) {
                DBGLOG("ngfx @ applying %zu patch for %zu kext", p, index);
                patcher.applyLookupPatch(&patch.patch);
                // Do not really care for the errors for now
                patcher.clearError();
            }
        }
    }
}

