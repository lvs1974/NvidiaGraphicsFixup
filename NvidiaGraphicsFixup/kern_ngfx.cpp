//
//  kern_ngfx.cpp
//  NvidiaGraphicsFixup
//
//  Copyright © 2017 lvs1974. All rights reserved.
//

#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <Headers/kern_iokit.hpp>

#include "kern_config.hpp"
#include "kern_ngfx.hpp"


static const char *kextAGDPolicy[] { "/System/Library/Extensions/AppleGraphicsControl.kext/Contents/PlugIns/AppleGraphicsDevicePolicy.kext/Contents/MacOS/AppleGraphicsDevicePolicy" };
static const char *kextGeForce[] { "/System/Library/Extensions/GeForce.kext/Contents/MacOS/GeForce" };
static const char *kextGeForceWeb[] { "/Library/Extensions/GeForceWeb.kext/Contents/MacOS/GeForceWeb", "/System/Library/Extensions/GeForceWeb.kext/Contents/MacOS/GeForceWeb" };

static KernelPatcher::KextInfo kextList[] {
    { "com.apple.driver.AppleGraphicsDevicePolicy",     kextAGDPolicy,   arrsize(kextAGDPolicy),  {true}, {}, KernelPatcher::KextInfo::Unloaded },
    { "com.apple.GeForce",        						kextGeForce,     arrsize(kextGeForce),    {},     {}, KernelPatcher::KextInfo::Unloaded },
    { "com.nvidia.web.GeForceWeb",     					kextGeForceWeb,  arrsize(kextGeForceWeb), {},     {}, KernelPatcher::KextInfo::Unloaded },
};

enum : size_t {
	KextAGDPolicy,
	KextGeForce,
	KextGeForceWeb
};

static size_t kextListSize {arrsize(kextList)};

// Only used in apple-driven callbacks
static NGFX *callbackNGFX = nullptr;


bool NGFX::init() {
	if (getKernelVersion() > KernelVersion::Mavericks && getKernelVersion() < KernelVersion::HighSierra)
	{
		LiluAPI::Error error = lilu.onPatcherLoad(
		  [](void *user, KernelPatcher &patcher) {
			  callbackNGFX = static_cast<NGFX *>(user);
			  callbackNGFX->processKernel(patcher);
		  }, this);

		if (error != LiluAPI::Error::NoError) {
			SYSLOG("ngfx", "failed to register onPatcherLoad method %d", error);
			return false;
		}
	} else {
		progressState |= ProcessingState::KernelRouted;
	}
    
    LiluAPI::Error error = lilu.onKextLoad(kextList, kextListSize,
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
        if (!config.nolibvalfix) {
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
                if (!(progressState & ProcessingState::GraphicsDevicePolicyPatched) && i == KextAGDPolicy)
                {
                    DBGLOG("ngfx", "found %s", kextList[i].id);
                    
                    const bool patch_vit9696 = (strstr(config.patch_list, "vit9696") != nullptr);
                    const bool patch_pikera  = (strstr(config.patch_list, "pikera")  != nullptr);
                    const bool patch_cfgmap  = (strstr(config.patch_list, "cfgmap")  != nullptr);
                    
                    if (patch_vit9696)
                    {
                        const uint8_t find[]    = {0xBA, 0x05, 0x00, 0x00, 0x00};
                        const uint8_t replace[] = {0xBA, 0x00, 0x00, 0x00, 0x00};
                        KextPatch kext_patch {
                            {&kextList[i], find, replace, sizeof(find), 1},
                            KernelVersion::MountainLion, KernelPatcher::KernelAny
                        };
                        applyPatches(patcher, index, &kext_patch, 1, "vit9696");
                    }
                    
                    if (patch_pikera)
                    {
                        const uint8_t find[]    = "board-id";
                        const uint8_t replace[] = "board-ix";
                        KextPatch kext_patch {
                            {&kextList[i], find, replace, strlen((const char*)find), 1},
                            KernelVersion::MountainLion, KernelPatcher::KernelAny
                        };
                        applyPatches(patcher, index, &kext_patch, 1, "pikera");
                    }
                    
                    if (patch_cfgmap)
                    {
                        auto method_address = patcher.solveSymbol(index, "__ZN25AppleGraphicsDevicePolicy5startEP9IOService");
                        if (method_address) {
                            DBGLOG("ngfx", "obtained __ZN25AppleGraphicsDevicePolicy5startEP9IOService");
                            patcher.clearError();
                            orgAgdpStart = reinterpret_cast<t_agdp_start>(patcher.routeFunction(method_address, reinterpret_cast<mach_vm_address_t>(AppleGraphicsDevicePolicy_start), true));
                            if (patcher.getError() == KernelPatcher::Error::NoError) {
                                DBGLOG("ngfx", "routed __ZN25AppleGraphicsDevicePolicy5startEP9IOService");
                            } else {
                                SYSLOG("ngfx", "failed to route __ZN25AppleGraphicsDevicePolicy5startEP9IOService");
                            }
                        } else {
                            SYSLOG("ngfx", "failed to resolve __ZN25AppleGraphicsDevicePolicy5startEP9IOService");
                        }
                    }
                    progressState |= ProcessingState::GraphicsDevicePolicyPatched;
                }
                else if (!(progressState & ProcessingState::GeForceRouted) && i == KextGeForce)
                {
                    if (!config.novarenderer) {
                        DBGLOG("ngfx", "found %s", kextList[i].id);
                        auto method_address = patcher.solveSymbol(index, "__ZN13nvAccelerator18SetAccelPropertiesEv");
                        if (method_address) {
                            DBGLOG("ngfx", "obtained __ZN13nvAccelerator18SetAccelPropertiesEv");
                            patcher.clearError();
                            orgSetAccelProperties = reinterpret_cast<t_set_accel_properties>(patcher.routeFunction(method_address, reinterpret_cast<mach_vm_address_t>(nvAccelerator_SetAccelProperties), true));
                            if (patcher.getError() == KernelPatcher::Error::NoError) {
                                DBGLOG("ngfx", "routed __ZN13nvAccelerator18SetAccelPropertiesEv");
                            } else {
                                SYSLOG("ngfx", "failed to route __ZN13nvAccelerator18SetAccelPropertiesEv");
                            }
                        } else {
                            SYSLOG("ngfx", "failed to resolve __ZN13nvAccelerator18SetAccelPropertiesEv");
                        }
                    }
                    
                    progressState |= ProcessingState::GeForceRouted;
                }
                else if (!(progressState & ProcessingState::GeForceWebRouted) && i == KextGeForceWeb)
                {
                    if (!config.novarenderer) {
                        DBGLOG("ngfx", "found %s", kextList[i].id);
                        auto method_address = patcher.solveSymbol(index, "__ZN19nvAcceleratorParent18SetAccelPropertiesEv");
                        if (method_address) {
                            DBGLOG("ngfx", "obtained __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
                            patcher.clearError();
                            orgSetAccelProperties = reinterpret_cast<t_set_accel_properties>(patcher.routeFunction(method_address, reinterpret_cast<mach_vm_address_t>(nvAccelerator_SetAccelProperties), true));
                            if (patcher.getError() == KernelPatcher::Error::NoError) {
                                DBGLOG("ngfx", "routed __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
                            } else {
                                SYSLOG("ngfx", "failed to route __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
                            }
                        } else {
                            SYSLOG("ngfx", "failed to resolve __ZN19nvAcceleratorParent18SetAccelPropertiesEv");
                        }
                    }
                    
                    progressState |= ProcessingState::GeForceWebRouted;
                }
			}
		}
	}
    
	// Ignore all the errors for other processors
	patcher.clearError();
}

void NGFX::nvAccelerator_SetAccelProperties(IOService* that)
{
    DBGLOG("ngfx", "SetAccelProperties is called");
    
    if (callbackNGFX && callbackNGFX->orgSetAccelProperties)
    {
        callbackNGFX->orgSetAccelProperties(that);

        if (!that->getProperty("IOVARendererID"))
        {
			uint8_t rendererId[] {0x08, 0x00, 0x04, 0x01};
            that->setProperty("IOVARendererID", rendererId, sizeof(rendererId));
            DBGLOG("ngfx", "set IOVARendererID to 08 00 04 01");
        }
        
        if (!that->getProperty("IOVARendererSubID"))
        {
			uint8_t rendererSubId[] {0x03, 0x00, 0x00, 0x00};
            that->setProperty("IOVARendererSubID", rendererSubId, sizeof(rendererSubId));
            DBGLOG("ngfx", "set IOVARendererSubID to value 03 00 00 00");
        }
    }

	auto gfx = that->getParentEntry(gIOServicePlane);
	int gl = gfx && gfx->getProperty("disable-metal");
	PE_parse_boot_argn("ngfxgl", &gl, sizeof(gl));

	if (gl) {
		DBGLOG("ngfx", "disabling metal support");
		that->removeProperty("MetalPluginClassName");
		that->removeProperty("MetalPluginName");
		that->removeProperty("MetalStatisticsName");
	}
}

bool NGFX::AppleGraphicsDevicePolicy_start(IOService *that, IOService *provider)
{
    bool result = false;
    
    DBGLOG("ngfx", "AppleGraphicsDevicePolicy::start is called");    
    if (callbackNGFX && callbackNGFX->orgAgdpStart)
    {
        char board_id[32];
        if (WIOKit::getComputerInfo(nullptr, 0, board_id, sizeof(board_id)))
        {
            DBGLOG("ngfx", "got board-id '%s'", board_id);
            auto dict = that->getPropertyTable();
            auto newProps = OSDynamicCast(OSDictionary, dict->copyCollection());
            OSDictionary *configMap = OSDynamicCast(OSDictionary, newProps->getObject("ConfigMap"));
            if (configMap != nullptr)
            {
                OSString *value = OSDynamicCast(OSString, configMap->getObject(board_id));
                if (value != nullptr)
                    DBGLOG("ngfx", "Current value for board-id '%s' is %s", board_id, value->getCStringNoCopy());
                if (!configMap->setObject(board_id, OSString::withCString("none")))
                    SYSLOG("ngfx", "Configuration for board-id '%s' can't be set, setObject was failed.", board_id);
                else
                    DBGLOG("ngfx", "Configuration for board-id '%s' has been set to none", board_id);
                that->setPropertyTable(newProps);
            }
            else
            {
                SYSLOG("ngfx", "ConfigMap key was not found in personalities");
                OSSafeReleaseNULL(newProps);
            }
        }
        
        result = callbackNGFX->orgAgdpStart(that, provider);
        DBGLOG("ngfx", "AppleGraphicsDevicePolicy::start returned %d", result);
    }
    
    return result;
}

int NGFX::csfg_get_platform_binary(void *fg)
{
    //DBGLOG("ngfx", "csfg_get_platform_binary is called"); // is called quite often
    
    if (callbackNGFX && callbackNGFX->org_csfg_get_platform_binary && callbackNGFX->csfg_get_teamid)
    {
        int result = callbackNGFX->org_csfg_get_platform_binary(fg);
        if (!result)
        {
            // Special case NVIDIA drivers
            const char *teamId = callbackNGFX->csfg_get_teamid(fg);
            if (teamId != nullptr && strcmp(teamId, kNvidiaTeamId) == 0)
            {
                DBGLOG("ngfx", "platform binary override for %s", kNvidiaTeamId);
                return 1;
            }
        }
        
        return result;
    }
    
    // Default to error
    return 0;
}

void NGFX::applyPatches(KernelPatcher &patcher, size_t index, const KextPatch *patches, size_t patchNum, const char* name) {
    DBGLOG("ngfx", "applying patch '%s' for %zu kext", name, index);
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

