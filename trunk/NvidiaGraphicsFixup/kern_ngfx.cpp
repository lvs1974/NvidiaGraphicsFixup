//
//  kern_ngfx.cpp
//  NvidiaGraphicsFixup
//
//  Copyright © 2017 lvs1974. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <Headers/kern_iokit.hpp>

#include "kern_ngfx.hpp"


static const char *kextGraphicsDevicePolicy[] { "/System/Library/Extensions/AppleGraphicsControl.kext/Contents/PlugIns/AppleGraphicsDevicePolicy.kext/Contents/MacOS/AppleGraphicsDevicePolicy" };
static const char *kextGraphicsDevicePolicyId { "com.apple.driver.AppleGraphicsDevicePolicy" };

static const char *kextGeForce[] { "/System/Library/Extensions/GeForce.kext/Contents/MacOS/GeForce" };
static const char *kextGeForceId { "com.apple.GeForce" };

static const char *kextGeForceWeb[] { "/Library/Extensions/GeForceWeb.kext/Contents/MacOS/GeForceWeb" };
static const char *kextGeForceWebId { "com.nvidia.web.GeForceWeb" };


static KernelPatcher::KextInfo kextList[] {
    { kextGraphicsDevicePolicyId, kextGraphicsDevicePolicy, 1, {true,  false}, {}, KernelPatcher::KextInfo::Unloaded },
    { kextGeForceId,              kextGeForce,              1, {false, false}, {}, KernelPatcher::KextInfo::Unloaded },
    { kextGeForceWebId,           kextGeForceWeb,           1, {false, false}, {}, KernelPatcher::KextInfo::Unloaded },
};

static size_t kextListSize {3};

// Only used in apple-driven callbacks
static NGFX *callbackNGFX = nullptr;


bool NGFX::init() {
    
    LiluAPI::Error error = lilu.onPatcherLoad(
        [](void *user, KernelPatcher &patcher) {
          callbackNGFX = static_cast<NGFX *>(user);
          callbackNGFX->processKernel(patcher);
        }, this);
    
    if (error != LiluAPI::Error::NoError) {
        SYSLOG("ngfx", "failed to register onPatcherLoad method %d", error);
        return false;
    }
    
	error = lilu.onKextLoad(kextList, kextListSize,
        [](void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
            callbackNGFX = static_cast<NGFX *>(user);
            callbackNGFX->processKext(patcher, index, address, size);
        }, this);
	
	if (error != LiluAPI::Error::NoError) {
		SYSLOG("ngfx", "failed to register onKextLoad method %d", error);
		return false;
	}
	
	return true;
}

void NGFX::deinit() {
}

void NGFX::processKernel(KernelPatcher &patcher) {
    if (!(progressState & ProcessingState::KernelRouted))
    {
        auto method_address = patcher.solveSymbol(KernelPatcher::KernelID, "_csfg_get_teamid");
        if (method_address) {
            DBGLOG("ngfx", "obtained _csfg_get_teamid");
            csfg_get_teamid = reinterpret_cast<t_csfg_get_teamid>(method_address);
            
            method_address = patcher.solveSymbol(KernelPatcher::KernelID, "_csfg_get_platform_binary");
            if (method_address ) {
                DBGLOG("ngfx", "obtained _csfg_get_platform_binary");
                patcher.clearError();
                org_csfg_get_platform_binary = reinterpret_cast<t_csfg_get_platform_binary>(patcher.routeFunction(method_address, reinterpret_cast<mach_vm_address_t>(csfg_get_platform_binary), true));
                if (patcher.getError() == KernelPatcher::Error::NoError) {
                    DBGLOG("ngfx", "routed _csfg_get_platform_binary");
                } else {
                    SYSLOG("ngfx", "failed to route _csfg_get_platform_binary");
                }
            } else {
                SYSLOG("ngfx", "failed to resolve _csfg_get_platform_binary");
            }
            
        } else {
            SYSLOG("ngfx", "failed to resolve _csfg_get_teamid");
        }
        
        progressState |= ProcessingState::KernelRouted;
    }
    
    // Ignore all the errors for other processors
    patcher.clearError();
}

void NGFX::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
	if (progressState != ProcessingState::EverythingDone) {
		for (size_t i = 0; i < kextListSize; i++) {
			if (kextList[i].loadIndex == index) {
                if (!(progressState & ProcessingState::GraphicsDevicePolicyPatched) && !strcmp(kextList[i].id, kextGraphicsDevicePolicyId))
                {
                    DBGLOG("ngfx", "found %s", kextGraphicsDevicePolicyId);
                    const uint8_t find[]    = {0xBA, 0x05, 0x00, 0x00, 0x00};
                    const uint8_t replace[] = {0xBA, 0x00, 0x00, 0x00, 0x00};
                    KextPatch kext_patch {
                        {&kextList[i], find, replace, sizeof(find), 1},
						KernelVersion::MountainLion, KernelPatcher::KernelAny
					};
                    applyPatches(patcher, index, &kext_patch, 1);
                    progressState |= ProcessingState::GraphicsDevicePolicyPatched;
                }
                else if (!(progressState & ProcessingState::GeForceRouted) && !strcmp(kextList[i].id, kextGeForceId))
                {
                    DBGLOG("ngfx", "found %s", kextGeForceId);
                    auto method_address = patcher.solveSymbol(index, "__ZN13nvAccelerator18SetAccelPropertiesEv");
                    if (method_address) {
                        DBGLOG("ngfx", "obtained __ZN13nvAccelerator18SetAccelPropertiesEv");
                        patcher.clearError();
                        orgSetAccelProperties = reinterpret_cast<t_set_accel_properties>(patcher.routeFunction(method_address, reinterpret_cast<mach_vm_address_t>(SetAccelProperties), true));
                        if (patcher.getError() == KernelPatcher::Error::NoError) {
                            DBGLOG("ngfx", "routed __ZN13nvAccelerator18SetAccelPropertiesEv");
                        } else {
                            SYSLOG("ngfx", "failed to route __ZN13nvAccelerator18SetAccelPropertiesEv");
                        }
                    } else {
                        SYSLOG("ngfx", "failed to resolve __ZN13nvAccelerator18SetAccelPropertiesEv");
                    }
                    
                    progressState |= ProcessingState::GeForceRouted;
                }
                else if (!(progressState & ProcessingState::GeForceWebRouted) && !strcmp(kextList[i].id, kextGeForceWebId))
                {
                    DBGLOG("ngfx", "found %s", kextGeForceWebId);
                    auto method_address = patcher.solveSymbol(index, "__ZN19nvAcceleratorParent18SetAccelPropertiesEv");
                    if (method_address) {
                        DBGLOG("ngfx", "obtained __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
                        patcher.clearError();
                        orgSetAccelProperties = reinterpret_cast<t_set_accel_properties>(patcher.routeFunction(method_address, reinterpret_cast<mach_vm_address_t>(SetAccelProperties), true));
                        if (patcher.getError() == KernelPatcher::Error::NoError) {
                            DBGLOG("ngfx", "routed __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
                        } else {
                            SYSLOG("ngfx", "failed to route __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
                        }
                    } else {
                        SYSLOG("ngfx", "failed to resolve __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
                    }
                    
                    progressState |= ProcessingState::GeForceWebRouted;
                }
			}
		}
	}
    
	// Ignore all the errors for other processors
	patcher.clearError();
}

void NGFX::SetAccelProperties(IOService* that)
{
    DBGLOG("ngfx", "SetAccelProperties is called");
    
    if (callbackNGFX && callbackNGFX->orgSetAccelProperties)
    {
        callbackNGFX->orgSetAccelProperties(that);

        int32_t rendererId = 0, rendererSubId = 0;
        if (!WIOKit::getOSDataValue(that, "IOVARendererID", rendererId))
        {
            rendererId = 0x1040008;
            that->setProperty("IOVARendererID", rendererId);
            DBGLOG("ngfx", "set IOVARendererID to value 0x%08x", rendererId);
        }
        
        if (!WIOKit::getOSDataValue(that, "IOVARendererSubID", rendererSubId))
        {
            rendererSubId = 3;
            that->setProperty("IOVARendererSubID", rendererSubId);
            DBGLOG("ngfx", "set IOVARendererID to value 0x%08x", rendererSubId);
        }
    }
}

int NGFX::csfg_get_platform_binary(void *fg)
{
    //DBGLOG("ngfx", "csfg_get_platform_binary is called"); // is called quite often
    
    if (callbackNGFX && callbackNGFX->org_csfg_get_platform_binary && callbackNGFX->csfg_get_teamid)
    {
        int result = callbackNGFX->org_csfg_get_platform_binary(fg);
        if (result)
        {
            // Special case NVIDIA drivers
            const char *teamId = callbackNGFX->csfg_get_teamid(fg);
            if (teamId != nullptr && strcmp(teamId, kNvidiaTeamId) == 0)
            {
                DBGLOG("ngfx", "platform binary override for %s", kNvidiaTeamId);
                return 0;
            }
        }
        
        return result;
    }
    
    // Default to error
    return 1;
}

void NGFX::applyPatches(KernelPatcher &patcher, size_t index, const KextPatch *patches, size_t patchNum) {
    DBGLOG("ngfx", "applying patches for %zu kext", index);
    for (size_t p = 0; p < patchNum; p++) {
        auto &patch = patches[p];
        if (patch.patch.kext->loadIndex == index) {
            if (patcher.compatibleKernel(patch.minKernel, patch.maxKernel)) {
                DBGLOG("ngfx", "applying %zu patch for %zu kext", p, index);
                patcher.applyLookupPatch(&patch.patch);
                // Do not really care for the errors for now
                patcher.clearError();
            }
        }
    }
}

